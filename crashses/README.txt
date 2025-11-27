Command line used to find this crash:

/home/ubuntu/xpgfuzz/afl-fuzz -d -i /home/ubuntu/experiments/in-ftp-x -o out-proftpd-xpgfuzz -x /home/ubuntu/experiments/ftp.dict -N tcp://127.0.0.1/21 -P FTP -D 10000 -q 3 -s 3 -E -K -m none -t 5000+ -c /home/ubuntu/experiments/clean ./proftpd -n -c /home/ubuntu/experiments/basic.conf -X

If you can't reproduce a bug outside of afl-fuzz, be sure to set the same
memory limit. The limit used for this fuzzing session was 0 B.

Need a tool to minimize test cases before investigating the crashes or sending
them to a vendor? Check out the afl-tmin that comes with the fuzzer!

Found any cool bugs in open-source tools using afl-fuzz? If yes, please drop
me a mail at <lcamtuf@coredump.cx> once the issues are fixed - I'd love to
add your finds to the gallery at:

  http://lcamtuf.coredump.cx/afl/

Thanks :-)
