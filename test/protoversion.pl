#!/usr/bin/env perl

use strict;
$|++;

use IPC::Open2;
use Session::Token;
use FindBin;
use lib "$FindBin::Bin";
use Utils;

die "usage: $0 <lang>" if @ARGV < 1;
my $harnessCmd = Utils::harnessTypeToCmd(shift) || die "please provide harness type (cpp, js, etc)";


my $expectedResp;

## Get expected response using protocol version 0

{
    my ($infile, $outfile);
    my $pid = open2($outfile, $infile, $harnessCmd);

    print $infile "item,12345,eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee\n";
    print $infile "seal\n";
    print $infile "msg,6000000200\n"; ## full range bound, empty IdList

    my $resp = <$outfile>;
    chomp $resp;

    $expectedResp = $resp;
}

## Client tries to use some hypothetical newer version, but falls back to version 0

{
    my ($infile, $outfile);
    my $pid = open2($outfile, $infile, $harnessCmd);

    print $infile "item,12345,eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee\n";
    print $infile "seal\n";

    print $infile "msg,61aabbccddeeff\n"; ## some new protocol message

    my $resp = <$outfile>;
    chomp $resp;

    ## 61: The bound timestamp, as varint. The value is 0x60 (preferred protocol version), but 1 is added as per timestamp protocol
    ## 00: The following ID has length 0
    ## 04: UnsupportedProtocolVersion mode
    die "bad upgrade response: $resp" unless $resp eq "msg,610004";

    ## Try again with protocol version 0
    print $infile "msg,6000000200\n"; ## full range bound, empty IdList

    $resp = <$outfile>;
    chomp $resp;
    die "didn't fall back to protocol version 0: $resp" unless $resp eq $expectedResp;
}

print "OK\n";
