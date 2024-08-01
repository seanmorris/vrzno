#!/usr/bin/env perl

use strict;
use warnings;

print "// ## GENERATED FILE ##\n\n";

while(<>) {
	if(/^##JS "(.+?)"/)
	{
		open(my $file, '<', $1);

		print "EM_ASM({\\\n";
		while (<$file>) {
			next if(m/^\s+\/\//);
			s/\/\/.+?$//;
			chomp;
			print;
			print "\\\n";
		}
		print "});\n";

	}
	else
	{
		print;
	}
}
