package Utils;

use strict;


sub harnessTypeToCmd {
    my $harnessType = shift;

    if ($harnessType eq 'cpp') {
        return './cpp/harness';
    } elsif ($harnessType eq 'js') {
        return 'node js/harness.js';
    } elsif ($harnessType eq 'rust') {
        return '../../rust-negentropy/target/debug/harness';
    }

    die "unknown harness type: $harnessType";
}


1;
