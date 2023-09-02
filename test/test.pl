#!/usr/bin/env perl

use strict;

unlink("negent-test.log");


my $langs = shift // 'cpp,js';


foreach my $lang (split /,/, $langs) {
    note("------LANG $lang ------");

    note("Full upload");
    run("RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=1 P2=0 P3=0 perl fuzz.pl $lang $lang");

    note("Full download");
    run("RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=0 P2=1 P3=0 perl fuzz.pl $lang $lang");

    note("Identical DBs");
    run("RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=0 P2=0 P3=1 perl fuzz.pl $lang $lang");

    note("Mixed");
    run("RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=1 P2=1 P3=5 perl fuzz.pl $lang $lang");
}





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
