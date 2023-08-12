#!/usr/bin/env perl

use strict;

unlink("negent-test.log");


note("Full upload");
run("RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=1 P2=0 P3=0 perl fuzz.pl cpp");

note("Full download");
run("RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=0 P2=1 P3=0 perl fuzz.pl cpp");

note("Identical DBs");
run("RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=0 P2=0 P3=1 perl fuzz.pl cpp");

note("Mixed");
run("RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=1 P2=1 P3=5 perl fuzz.pl cpp");





########

sub run {
    my $cmd = shift;

    print "RUN: $cmd\n";

    system("echo 'RUN: $cmd' >>negent-test.log");
    system("$cmd >>negent-test.log 2>&1") && die "test failure";
    system("echo '----------' >>negent-test.log");
}

sub note {
    my $note = shift;

    print "NOTE: $note\n";

    system("echo 'NOTE: $note' >>negent-test.log");
}
