#!/usr/bin/env perl

use strict;

unlink("negent-test.log");


my $langs = shift // 'cpp,js';
my @langs = split /,/, $langs;


## Compat tests (verify langs are compatible with eachother)

{
    my $allOpts = [
        'RECS=50000 P1=1 P2=0 P3=0 NODUPS=1',
        'RECS=50000 P1=0 P2=1 P3=0 NODUPS=1',
        'RECS=50000 P1=0 P2=0 P3=1 NODUPS=1',
        'RECS=50000 P1=2 P2=2 P3=7 NODUPS=1',

        'RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=1 P2=0 P3=0',
        'RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=0 P2=1 P3=0',
        'RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=0 P2=0 P3=1',
        'RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=1 P2=1 P3=5',

        'CLUSTERED=1 FRAMESIZELIMIT1=50000 FRAMESIZELIMIT2=50000',
        'CLUSTERED=1 FRAMESIZELIMIT1=50000 FRAMESIZELIMIT2=50000 P1=1 P2=1 P3=1',
        'CLUSTERED=1 NODUPS=1',
    ];

    foreach my $lang1 (@langs) {
        foreach my $lang2 (@langs) {
            foreach my $opts (@$allOpts) {
                note("------ INTEROP $lang1 / $lang2 : $opts ------");
                run("$opts perl fuzz.pl $lang1 $lang2");
            }
        }
    }
}


## Delta tests (ensure output from all langs are byte-for-byte identical)

if (@langs >= 2) {
    my @otherLangs = @langs;
    my $firstLang = shift @otherLangs;

    my $allOpts = [
        'RECS=10000 P1=1 P2=0 P3=0',
        'RECS=10000 P1=0 P2=1 P3=0',
        'RECS=10000 P1=0 P2=0 P3=1',

        'RECS=10000 P1=1 P2=1 P3=10',
        'RECS=100000 FRAMESIZELIMIT1=60000 FRAMESIZELIMIT2=500000 P1=1 P2=1 P3=10',
        'RECS=200000 FRAMESIZELIMIT1=30000 FRAMESIZELIMIT2=200000 P1=1 P2=1 P3=10',
    ];

    foreach my $opts (@$allOpts) {
        foreach my $lang (@otherLangs) {
            note("------- DELTA $firstLang / $lang : $opts ------");

            my $res = system(
                "/bin/bash",
                "-c",
                qq{diff -u <(DELTATRACE=1 $opts perl fuzz.pl $firstLang $firstLang | grep DELTATRACE) <(DELTATRACE=1 $opts perl fuzz.pl $lang $lang | grep DELTATRACE)},
            );

            die "Difference detected between $firstLang / $lang" if $res;
        }
    }
}



## Protocol upgrade tests

foreach my $lang (@langs) {
    note("------- PROTO UPGRADE $lang -------");
    run("perl protoversion.pl $lang");
}



########

sub run {
    my $cmd = shift;

    print "RUN: $cmd\n";

    system("echo 'RUN: $cmd' >>negent-test.log");
    system("$cmd >>negent-test.log 2>&1") && die "test failure (see negent-test.log file)";
    system("echo '----------' >>negent-test.log");
}

sub note {
    my $note = shift;

    print "NOTE: $note\n";

    system("echo 'NOTE: $note' >>negent-test.log");
}
