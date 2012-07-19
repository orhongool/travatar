#!/usr/bin/perl

use strict;
use utf8;
use Getopt::Long;
use List::Util qw(sum min max shuffle);
binmode STDIN, ":utf8";
binmode STDOUT, ":utf8";
binmode STDERR, ":utf8";

my $TOP_N = 10;
my $SRC_MIN_FREQ = 2;
GetOptions(
    "top-n=i" => \$TOP_N,
    "src-min-freq=i" => \$SRC_MIN_FREQ
);

if(@ARGV != 0) {
    print STDERR "Usage: $0 < INPUT > OUTPUT\n";
    exit 1;
}

sub print_counts {
    my $id = shift;
    my $counts = shift;
    my $sum = sum(values %$counts);
    return if $sum < $SRC_MIN_FREQ;
    my @keys = keys %$counts;
    if(@keys > $TOP_N) {
        @keys = sort { $counts->{$b} <=> $counts->{$a} } @keys;
        @keys = @keys[0 .. $TOP_N-1];
    }
    for(sort @keys) {
        my $words = 0;
        while(/"[^ ]*?"/g) { $words++; }
        printf "$id ||| $_ ||| Pegf=%f ppen=-1", log($counts->{$_}/$sum);
        print " w=$words" if($words);
        print "\n";
    }
}

my (%counts, $curr_id);
while(<STDIN>) {
    chomp;
    my @arr = split(/ \|\|\| /);
    if($arr[0] ne $curr_id) {
        print_counts($curr_id, \%counts);
        %counts = ();
        $curr_id = $arr[0];
    }
    $counts{$arr[1]} += $arr[2];
}
print_counts($curr_id, \%counts);