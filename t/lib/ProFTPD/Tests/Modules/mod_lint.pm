package ProFTPD::Tests::Modules::mod_lint;

use lib qw(t/lib);
use base qw(ProFTPD::TestSuite::Child);
use strict;

use Carp;
use File::Path qw(mkpath);
use File::Spec;
use IO::Handle;

use ProFTPD::TestSuite::FTP;
use ProFTPD::TestSuite::Utils qw(:auth :config :running :test :testsuite);

$| = 1;

my $order = 0;

my $TESTS = {
  lint_configfile => {
    order => ++$order,
    test_class => [qw(forking)],
  },

  lint_restart => {
    order => ++$order,
    test_class => [qw(forking)],
  },

  lint_dso => {
    order => ++$order,
    test_class => [qw(feature_dso forking)],
  },

};

sub new {
  return shift()->SUPER::new(@_);
}

sub list_tests {
  return testsuite_get_runnable_tests($TESTS);
}

# Support functions

sub create_test_dir {
  my $setup = shift;
  my $sub_dir = shift;

  mkpath($sub_dir);

  # Make sure that, if we're running as root, that the sub directory has
  # permissions/privs set for the account we create
  if ($< == 0) {
    unless (chmod(0755, $sub_dir)) {
      die("Can't set perms on $sub_dir to 0755: $!");
    }

    unless (chown($setup->{uid}, $setup->{gid}, $sub_dir)) {
      die("Can't set owner of $sub_dir to $setup->{uid}/$setup->{gid}: $!");
    }
  }
}

sub server_test_config {
  my $config_file = shift;
  my $proftpd_bin = ProFTPD::TestSuite::Utils::get_proftpd_bin();

  my $quiet = '-q';
  if ($ENV{TEST_VERBOSE}) {
    $quiet = '';
  }

  my $cmd = "$proftpd_bin $quiet -t -c $config_file";

  if ($ENV{TEST_VERBOSE}) {
    $cmd .= ' -d10';
  }

  # Any testing errors will be written to stderr, but we capture stdout.
  $cmd .= ' 2>&1';

  if ($ENV{TEST_VERBOSE}) {
    print STDERR "Testing config using: $cmd\n";
  }

  my @output = `$cmd`;

  foreach my $line (@output) {
    chomp($line);

    if ($ENV{TEST_VERBOSE}) {
      print STDERR "# $line\n";
    }

    if ($line =~ /fatal/) {
      $line =~ s/^.*?fatal/fatal/;
      croak($line);
    }
  }
}

sub assert_lint_config_ok {
  my $log_file = shift;
  my $lint_config_file = shift;

  if (open(my $fh, "< $log_file")) {
    my $failed_emitting = 0;
    my $no_matching = 0;

    while (my $line = <$fh>) {
      if ($line =~ /failed to emit config file/) {
        $failed_emitting = 1;
        last;
      }

      if ($line =~ /found no matching parsed line for/) {
        $no_matching = 1;
        last;
      }
    }

    close($fh);

    croak("Failed to generate lint config file") unless $failed_emitting == 0;
    croak("Failed to properly reconstruct full config due to unknown config_rec name") unless $no_matching == 0;

  } else {
    croak("Can't read $log_file: $!");
  }

  if (open(my $fh, "< $lint_config_file")) {
    while (my $line = <$fh>) {
      chomp($line);

      if ($ENV{TEST_VERBOSE}) {
        print STDERR "$line\n";
      }
    }

    close($fh);

    # The real test: will ProFTPD read what we just wrote?
    server_test_config($lint_config_file);

  } else {
    croak("Can't read $lint_config_file: $!");
  }

  1;
}

# Test cases

sub lint_configfile {
  my $self = shift;
  my $tmpdir = $self->{tmpdir};
  my $setup = test_setup($tmpdir, 'lint');

  my $lint_config_file = File::Spec->rel2abs("$tmpdir/generated.conf");

  my $config = {
    PidFile => $setup->{pid_file},
    ScoreboardFile => $setup->{scoreboard_file},
    SystemLog => $setup->{log_file},
    TraceLog => $setup->{log_file},
    Trace => 'lint:20',

    AuthUserFile => $setup->{auth_user_file},
    AuthGroupFile => $setup->{auth_group_file},

    AllowOverwrite => 'on',
    AllowStoreRestart => 'on',

    IfModules => {
      'mod_delay.c' => {
        DelayEngine => 'off',
      },

      'mod_lint.c' => {
        LintConfigFile => $lint_config_file,
      },
    },
  };

  my ($port, $config_user, $config_group) = config_write($setup->{config_file},
    $config);

  server_start($setup->{config_file}, $setup->{pid_file});
  server_stop($setup->{pid_file});

  my $ex;

  eval { assert_lint_config_ok($setup->{log_file}, $lint_config_file) };
  if ($@) {
    $ex = $@;
  }

  test_cleanup($setup->{log_file}, $ex);
}

sub lint_restart {
  my $self = shift;
  my $tmpdir = $self->{tmpdir};
  my $setup = test_setup($tmpdir, 'lint');

  my $lint_config_file = File::Spec->rel2abs("$tmpdir/generated.conf");

  my $config = {
    PidFile => $setup->{pid_file},
    ScoreboardFile => $setup->{scoreboard_file},
    SystemLog => $setup->{log_file},
    TraceLog => $setup->{log_file},
    Trace => 'lint:20',

    AuthUserFile => $setup->{auth_user_file},
    AuthGroupFile => $setup->{auth_group_file},

    IfModules => {
      'mod_lint.c' => {
        LintConfigFile => $lint_config_file,
      },
    },
  };

  my ($port, $config_user, $config_group) = config_write($setup->{config_file},
    $config);

  server_start($setup->{config_file}, $setup->{pid_file});
  server_restart($setup->{pid_file});
  server_stop($setup->{pid_file});

  my $ex;

  eval { assert_lint_config_ok($setup->{log_file}, $lint_config_file) };
  if ($@) {
    $ex = $@;
  }

  test_cleanup($setup->{log_file}, $ex);
}

sub lint_dso {
  my $self = shift;
  my $tmpdir = $self->{tmpdir};
  my $setup = test_setup($tmpdir, 'lint');

  my $lint_config_file = File::Spec->rel2abs("$tmpdir/generated.conf");
  my $dso_path = File::Spec->rel2abs("$tmpdir/libexec");
  mkpath($dso_path);

  my $config = {
    PidFile => $setup->{pid_file},
    ScoreboardFile => $setup->{scoreboard_file},
    SystemLog => $setup->{log_file},
    TraceLog => $setup->{log_file},
    Trace => 'lint:20',

    AuthUserFile => $setup->{auth_user_file},
    AuthGroupFile => $setup->{auth_group_file},

    AllowOverwrite => 'on',
    AllowStoreRestart => 'on',

    IfModules => {
      'mod_delay.c' => {
        DelayEngine => 'off',
      },

      'mod_dso.c' => {
        ModulePath => $dso_path,
      },

      'mod_lint.c' => {
        LintConfigFile => $lint_config_file,
      },
    },
  };

  my ($port, $config_user, $config_group) = config_write($setup->{config_file},
    $config);

  server_start($setup->{config_file}, $setup->{pid_file});
  server_stop($setup->{pid_file});

  my $ex;

  eval {
    assert_lint_config_ok($setup->{log_file}, $lint_config_file);

    if (open(my $fh, "< $lint_config_file")) {
      my $ok = 0;

      while (my $line = <$fh>) {
        chomp($line);

        if ($line =~ /^ModulePath /) {
          $ok = 1;
          last;
        }
      }

      close($fh);

      $self->assert($ok,
        test_msg("Did not see expected ModulePath directive in generated config"));

    } else {
      die("Can't read $lint_config_file: $!");
    }
  };
  if ($@) {
    $ex = $@;
  }

  test_cleanup($setup->{log_file}, $ex);
}

1;
