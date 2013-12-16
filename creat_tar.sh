version=`grep argp_program_version src/giis-ext4.c | cut -f2 -d'=' | cut -f3 -d' '`
git archive --prefix=giis-ext4/ master | bzip2 >giis-ext4_${version}.tar.bz2
