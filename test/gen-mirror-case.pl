#!/usr/bin/env perl
# Writes client.txt and server.txt in fuzz.pl's SET1/SET2 format: a case where the initiator
# holds the newest items, which a frame-limited sync never reports. Then, in test/:
#   SET1=client.txt SET2=server.txt FRAMESIZELIMIT1=4096 FRAMESIZELIMIT2=4767 perl fuzz.pl cpp cpp
use strict;
use warnings;

my $seq = 1;
sub id { return unpack('H*', pack('Q<', $seq++)) . ('00' x 24) }

my (@client, @server);
for my $i (0 .. 1499) { my $item = (1000 + $i * 10) . ',' . id(); push @client, $item; push @server, $item }
push @server, (1000 + $_ * 1000 + 5) . ',' . id() for 0 .. 14;
for my $b (0 .. 1) { push @client, (1000 + $b * 1000 + 201 + $_ % 8) . ',' . id() for 0 .. 34 }
for my $j (0 .. 39) { my $item = (15900 + $j * 2) . ',' . id(); push @client, $item; push @server, $item }
push @server, '15999,' . id();
push @client, (50000 + $_ * 10) . ',' . id() for 0 .. 109;    # above everything the server holds

for (['mirror-client.txt', \@client], ['mirror-server.txt', \@server]) {
    open my $fh, '>', $_->[0] or die "$_->[0]: $!";
    print $fh "$_\n" for @{ $_->[1] };
    close $fh;
}
