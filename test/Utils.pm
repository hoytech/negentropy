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
    } elsif ($harnessType eq 'go') {
        return 'go run go/harness.go';
    } elsif ($harnessType eq 'go-nostr') {
        return "bash -c 'cd go-nostr && go run .'";
    } elsif ($harnessType eq 'csharp') {
        return "./csharp/bin/Debug/net8.0/Harness";
    } elsif ($harnessType eq 'kotlin') {
        return "kotlin -classpath ../negentropy-kmp/negentropy/build/libs/negentropy-jvm-1.0.0.jar com.vitorpamplona.negentropy.MainKt";
    }

    die "unknown harness type: $harnessType";
}


1;
