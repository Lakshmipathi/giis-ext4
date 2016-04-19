				# gET iT i sAY. giis for ext4 (giis-ext4)
				   version.2.0
	
+About giis

+How to install

+User Guide and Documents


### About giis:

Linux Desktop comes with Trash Bin for GUI. But when you delete a file from commandline or delete a file via programs. They will be deleted forever! giis-ext4 acts as a recycle bin in such cases. 

`gET iT i sAY.giis-ext4` is a text based file recovery tool.Once you install giis-ext4,files on your disk can be recovered using giis-ext4. But Those files which deleted before the installation of giis-ext4 can't be recovered using giis-ext4.User has the option to choose directories too. Add only those directories where you will store important data.(adding directories like /tmp gives giis-ext4 overhead and installation will take long time).It can recovers a deleted file and restores in it's original directory,if path exists.It recoversdropped database tables.

### Screencast:
Tip,to save 6 minutes of your life, During screen-cast after 1:08 you can skip to 7:38. If you interested in boring recovery messages,feel free to watch the whole screen-cast :)

[![asciicast](https://asciinema.org/a/43t9t02wyg1r8hxhdo8w0533o.png)](https://asciinema.org/a/43t9t02wyg1r8hxhdo8w0533o)

### How it works?

During installation, giis-ext4 stores file attributes in a sqlite-db. These attribute include output from `stat` command along 
with critical data block address. You can view this database content from `/usr/local/giis/db/giis-db`

At specific time interval, giis-ext4 takes snapshot of pre-configured directories and updates the database. It also searches for deleted files and restores them back into `/usr/local/giis/trash` location.

### How reliable is giis-ext4?

As long as the freed data blocks are not re-used by kernel, giis-ext4 will work. Since giis-ext4 doesn't rely on journal entries, even when the system is rebooted,  giis-ext4 can recover the file. 

giis-ext4 can't recover any file ,(a) if its data blocks are re-used (b) if the file attributes doesn't present in its database. So itsnot 100% fool-proof. At the moment, it works only with ext4 file system.


### Future plans:

- Provide per user trash location.
- Include ext3 support in giis-ext4  ( rename the tool as giis)
- Support for Btrfs! 


### How to install:

see INSTALL file

### User Guide and Documents

For more screencasting/manuals,checkout  www.giis.co.in

### Magazine references:

- Linux For You (India/2008): http://www.giis.co.in/LFY.png
- Linux Format (England/2011) : http://giis.co.in/giis_LXF.jpg
- c't(Germany/2014) : http://giis.co.in/ct_pg1_Jan_2014.jpg
- c't : http://giis.co.in/ct_pg2_Jan_2014.jpg


You can get me at  <lakshmipathi.g@giis.co.in>

+Lakshmipathi.G
www.giis.co.in
+Mar 29,2016.
