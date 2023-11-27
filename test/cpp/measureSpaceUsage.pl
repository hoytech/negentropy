system(qq{ perl -pi -e 's/MIN_ITEMS = \\d+/MIN_ITEMS = 2/' ../../cpp/negentropy/storage/btree/core.h });
system(qq{ perl -pi -e 's/REBALANCE_THRESHOLD = \\d+/REBALANCE_THRESHOLD = 4/' ../../cpp/negentropy/storage/btree/core.h });
system(qq{ perl -pi -e 's/MAX_ITEMS = \\d+/MAX_ITEMS = 6/' ../../cpp/negentropy/storage/btree/core.h });

for (my $i = 6; $i < 128; $i += 2) {
    print "DOING ITER $i\n";
    system(qq{ perl -pi -e 's/MAX_ITEMS = \\d+/MAX_ITEMS = $i/' ../../cpp/negentropy/storage/btree/core.h });
    system("rm -f measureSpaceUsage && make measureSpaceUsage && rm -f testdb/data.mdb && ./measureSpaceUsage >> measureSpaceUsage.log");
}
