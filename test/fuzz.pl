#!/usr/bin/env perl

use strict;
$|++;

use IPC::Open2;
use Session::Token;
use FindBin;
use lib "$FindBin::Bin";
use Utils;

die "usage: $0 <lang1> <lang2>" if @ARGV < 2;
my $harnessCmd1 = Utils::harnessTypeToCmd(shift) || die "please provide harness type (cpp, js, etc)";
my $harnessCmd2 = Utils::harnessTypeToCmd(shift) || die "please provide harness type (cpp, js, etc)";


my $idSize = 32;
srand($ENV{SEED} || 0);
my $stgen = Session::Token->new(seed => "\x00" x 1024, alphabet => '0123456789abcdef', length => $idSize * 2);


my $minRecs = $ENV{MIN_RECS} // 1;
my $maxRecs = $ENV{MAX_RECS} // 10_000;
die "MIN_RECS > MAX_RECS" if $minRecs > $maxRecs;
$minRecs = $maxRecs = $ENV{RECS} if $ENV{RECS};

my $prob1 = $ENV{P1} // 1;
my $prob2 = $ENV{P2} // 1;
my $prob3 = $ENV{P3} // 98;

{
    my $total = $prob1 + $prob2 + $prob3;
    die "zero prob" if $total == 0;
    $prob1 = $prob1 / $total;
    $prob2 = $prob2 / $total;
    $prob3 = $prob3 / $total;
}


my $ids1 = {};
my $ids2 = {};

my ($pid1, $pid2);
my ($infile1, $infile2);
my ($outfile1, $outfile2);

{
    local $ENV{FRAMESIZELIMIT};
    $ENV{FRAMESIZELIMIT} = $ENV{FRAMESIZELIMIT1} if defined $ENV{FRAMESIZELIMIT1};
    $pid1 = open2($outfile1, $infile1, $harnessCmd1);
}

{
    local $ENV{FRAMESIZELIMIT};
    $ENV{FRAMESIZELIMIT} = $ENV{FRAMESIZELIMIT2} if defined $ENV{FRAMESIZELIMIT2};
    $pid2 = open2($outfile2, $infile2, $harnessCmd2);
}

my $num = $minRecs + rnd($maxRecs - $minRecs);

for (1..$num) {
    my $created = 1677970534 + rnd($num);
    my $id = $stgen->get;

    my $modeRnd = rand();

    if ($modeRnd < $prob1) {
        print $infile1 "item,$created,$id\n";
        $ids1->{$id} = 1;
    } elsif ($modeRnd < $prob1 + $prob2) {
        print $infile2 "item,$created,$id\n";
        $ids2->{$id} = 1;
    } else {
        print $infile1 "item,$created,$id\n";
        print $infile2 "item,$created,$id\n";
        $ids1->{$id} = 1;
        $ids2->{$id} = 1;
    }
}

print $infile1 "seal\n";
print $infile2 "seal\n";

print $infile1 "initiate\n";


my $round = 0;
my $totalUp = 0;
my $totalDown = 0;

while (1) {
    my $msg = <$outfile1>;
    chomp $msg;
    print "[1]: $msg\n" if $ENV{DEBUG};

    if ($msg =~ /^(have|need),(\w+)/) {
        my ($action, $id) = ($1, $2);

        if ($action eq 'need') {
            die "duplicate insert of $action,$id" if $ids1->{$id};
            $ids1->{$id} = 1;
        } elsif ($action eq 'have') {
            die "duplicate insert of $action,$id" if $ids2->{$id};
            $ids2->{$id} = 1;
        }

        next;
    } elsif ($msg =~ /^msg,(\w*)/) {
        my $data = $1;
        print "DELTATRACE1 $data\n" if $ENV{DELTATRACE};
        print $infile2 "msg,$data\n";

        my $bytes = length($data) / 2;
        $totalUp += $bytes;
        print "[$round] CLIENT -> SERVER: $bytes bytes\n";
    } elsif ($msg =~ /^done/) {
        last;
    } else {
        die "unexpected line from 1: '$msg'";
    }

    $msg = <$outfile2>;
    print "[2]: $msg\n" if $ENV{DEBUG};

    if ($msg =~ /^msg,(\w*)/) {
        my $data = $1;
        print "DELTATRACE2 $data\n" if $ENV{DELTATRACE};
        print $infile1 "msg,$data\n";

        my $bytes = length($data) / 2;
        $totalDown += $bytes;
        print "[$round] SERVER -> CLIENT: $bytes bytes\n";
    } else {
        die "unexpected line from 2: $msg";
    }

    $round++;
}

kill 'KILL', $pid1, $pid2;

for my $id (keys %$ids1) {
    die "$id not in ids2" if !$ids2->{$id};
}

for my $id (keys %$ids2) {
    die "$id not in ids1" if !$ids1->{$id};
}

print "UP: $totalUp bytes, DOWN: $totalDown bytes\n";

print "\n-----------OK-----------\n";


sub rnd {
    my $n = shift;
    return int(rand() * $n);
}
