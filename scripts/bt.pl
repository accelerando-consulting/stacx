#!/usr/bin/perl
use Modern::Perl;
my $addrtoline =
    "~/ard/hardware/espressif/esp32/tools/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-addr2line";
my $addr;
my $loc;
my $cmd;
while (<>) {
    next unless /^Backtrace:/;
    for $addr (grep {/^0x/} split) {
	$loc = (split(/:/, $addr, 2))[0];
	print "$addr: ";
	$cmd = "$addrtoline  -pfiaC  -e build/$ENV{BOARD}/$ENV{PROGRAM}.ino.elf -a $loc";
	#say STDERR $cmd;
	system("$cmd");
    }
    say "\n";
}
	    
