#!/sbin/sh

# nandroid v2.2.2 - an Android backup tool for the G1 by infernix and brainaid
# restore capability added by cyanogen

# pensive modified to allow to add prefixes to backups, and to restore specific backups
# pensive added the ability to exclude various images from the restore/backup operations, allows to preserve the newer
# recovery image if an older backup is being restored or to preserve user data. Also, saves space by not backing up
# partitions which change rarely.
# pensive added compressing backups and restoring compressed backups
# pensive added fetching system updates directly from the web into /sdcard/update.zip
# pensive added fetching system updates directly from the web into /cache and applying it.
# pensive added moving *update*.zip from /sdcard/download where a browser puts it to /sdcard/update.zip
# pensive added deletion of stale backups
# pensive added backup for ext partition on the sdcard to switch roms
# pensive added composite options --save NAME and --switchto NAME to switch ROMS
# pensive added list backup anywhere on the sdcard
# pensive added list updates (more precisely *.zip) anywhere on the sdcard
# Amon_RA : ext restore -> added check if ext backup is existing
# Amon_RA : ext restore -> added check if ext parition is existing
# Amon_RA -> added wimax restore/backup

# Requirements:

# - a modded android in recovery mode (JF 1.3 will work by default)
# - adb shell as root in recovery mode if not using a pre-made recovery image
# - busybox in recovery mode
# - dump_image compiled and in path on phone
# - mkyaffs2image compiled and installed in path on phone
# - flash_image compiled and in path on phone
# - unyaffs compiled and in path on phone
# - for [de]compression needs gzip or bzip2, part of the busybox
# - wget for the wireless updates

# Reference data:

# dev:    size   erasesize  name
#mtd0: 00040000 00020000 "misc"
#mtd1: 00500000 00020000 "recovery"
#mtd2: 00280000 00020000 "boot"
#mtd3: 04380000 00020000 "system"
#mtd4: 04380000 00020000 "cache"
#mtd5: 04ac0000 00020000 "userdata"
#mtd6 is everything, dump splash1 with: dd if=/dev/mtd/mtd6ro of=/sdcard/splash1.img skip=19072 bs=2048 count=150

# We don't dump misc or cache because they do not contain any useful data that we are aware of at this time.

# Logical steps (v2.2.1):
#
# 0.  test for a target dir and the various tools needed, if not found then exit with error.
# 1.  check "adb devices" for a device in recovery mode. set DEVICEID variable to the device ID. abort when not found.
# 2.  mount system and data partitions read-only, set up adb portforward and create destdir
# 3.  check free space on /cache, exit if less blocks than 20MB free
# 4.  push required tools to device in /cache
# 5   for partitions boot recovery misc:
# 5a  get md5sum for content of current partition *on the device* (no data transfered)
# 5b  while MD5sum comparison is incorrect (always is the first time):
# 5b1 dump current partition to a netcat session
# 5b2 start local netcat to dump image to current dir
# 5b3 compare md5sums of dumped data with dump in current dir. if correct, contine, else restart the loop (6b1)
# 6   for partitions system data:
# 6a  get md5sum for tar of content of current partition *on the device* (no data transfered)
# 6b  while MD5sum comparison is incorrect (always is the first time):
# 6b1 tar current partition to a netcat session
# 6b2 start local netcat to dump tar to current dir
# 6b3 compare md5sums of dumped data with dump in current dir. if correct, contine, else restart the loop (6b1)
# 6c  if i'm running as root:
# 6c1 create a temp dir using either tempdir command or the deviceid in /tmp
# 6c2 extract tar to tempdir
# 6c3 invoke mkyaffs2image to create the img
# 6c4 clean up
# 7.  remove tools from device /cache
# 8.  umount system and data on device
# 9.  print success.


DEVICEID=foo
RECOVERY=foo

SUBNAME=""
NORECOVERY=0
NOBOOT=0
NODATA=0
NOSYSTEM=0
NOMISC=0
NOCACHE=0
NOSPLASH1=0
NOSPLASH2=0
EXT=0

COMPRESS=0
GETUPDATE=0
RESTORE=0
BACKUP=0
DELETE=0
WEBGET=0
LISTBACKUP=0
LISTUPDATE=0
AUTOREBOOT=0
AUTOAPPLY=0
ITSANUPDATE=0
ITSANIMAGE=0
WEBGETSOURCE=""
WEBGETTARGET="/sdcard"

DEFAULTUPDATEPATH="/sdcard/download"

DEFAULTWEBUPDATE=http://n0rp.chemlab.org/android/update-cm-3.6.8.1-signed.zip
# There really should be a clone link always pointing to the latest
#DEFAULTWEBUPDATE="http://n0rp.chemlab.org/android/latestupdate-signed.zip"
DEFAULTWEBIMAGE=http://n0rp.chemlab.org/android/cm-recovery-1.4-signed.zip

# Set up the default (for pensive at least) nameservers
NAMESERVER1=64.46.128.3
NAMESERVER2=64.46.128.4

# WiFi works, rmnet0 setup ???
# Do not know how to start the rmnet0 interface in recovery
# If in normal mode "ifconfig rmnet0 down" kills rild too
# /system/bin/rild& exits immediately, todo?


DEVICEID=`cat /proc/cmdline | sed "s/.*serialno=//" | cut -d" " -f1`

# This is the default repository for backups
BACKUPPATH="/sdcard/nandroid/$DEVICEID"


# Boot, Cache, Data, ext, Misc, Recovery, System, Splash1, Splash2
# BACKUPLEGEND, If all the partitions are backed up it should be "CBDEMRS12"
# Enables the user to figure at a glance what is in the backup
BACKUPLEGEND=""

# Normally we want tar to be verbose for confidence building.
TARFLAGS="v"

DEFAULTCOMPRESSOR=gzip
DEFAULTEXT=.gz
DEFAULTLEVEL="-1"

ASSUMEDEFAULTUSERINPUT=0

echo2log()
{
    if [ -e /cache/recovery/log ]; then
	echo $1 >>/cache/recovery/log
    else
	echo $1 >/cache/recovery/log
    fi
}


if [ "`echo $0 | grep /sbin/nandroid-mobile.sh`" == "" ]; then
    cp $0 /sbin
    chmod 755 /sbin/`basename $0`
    exec /sbin/`basename $0` $@
fi


# Hm, have to handle old options for the current UI
case $1 in
    restore)
        shift
        RESTORE=1
        ;;
    backup)
        shift
        BACKUP=1
        ;;
    --)
        ;;
esac

ECHO=echo
OUTPUT=""

for option in $(getopt --name="nandroid-mobile v2.2.2" -l norecovery -l noboot -l nodata -l nosystem -l nocache -l nomisc -l nosplash1 -l nowimax -l nosplash2 -l subname: -l backup -l restore -l compress -l getupdate -l delete -l path -l webget: -l webgettarget: -l nameserver: -l nameserver2: -l bzip2: -l defaultinput -l autoreboot -l autoapplyupdate -l ext -l android_secure -l save: -l switchto: -l listbackup -l listupdate -l silent -l quiet -l help -- "cbruds:p:eaql" "$@"); do
    case $option in
        --silent)
            ECHO=echo2log
            ASSUMEDEFAULTUSERINPUT=1
            TARFLAGS=""
            OUTPUT=>>/cache/recovery/log
            shift
            ;;
        --quiet)
            ECHO=echo2log
            ASSUMEDEFAULTUSERINPUT=1
            TARFLAGS=""
            OUTPUT=>>/cache/recovery/log
            shift
            ;;
        -q)
            ECHO=echo2log
            ASSUMEDEFAULTUSERINPUT=1
            TARFLAGS=""
            OUTPUT=>>/cache/recovery/log
            shift
            ;;
        --help)
            ECHO=echo
            $ECHO "Usage: $0 {--backup|--restore|--getupdate|--delete|--compress|--bzip2:ARG|--webget:URL|--listbackup|--listupdate} [options]"
            $ECHO ""
            $ECHO "--help                     Display this help"
            $ECHO ""
            $ECHO "-s | --subname: SUBSTRING  In case of --backup the SUBSTRING is"
            $ECHO "                           the prefix used with backup name"
            $ECHO "                           in case of --restore or -c|--compress|--bzip2 or"
            $ECHO "                           --delete SUBSTRING specifies backups on which to"
            $ECHO "                           operate"
            $ECHO ""
            $ECHO "-u | --getupdate           Will search /sdcard/download for files named"
            $ECHO "                           *update*.zip, will prompt the user"
            $ECHO "                           to narrow the choice if more than one is found,"
            $ECHO "                           and then move the latest, if not unique,"
            $ECHO "                           to sdcard root /sdcard with the update.zip name"
            $ECHO "                           It is assumed the browser was used to put the *.zip"
            $ECHO "                           in the /sdcard/download folder. -p|--path option"
            $ECHO "                           would override /sdcard/download with the path of your"
            $ECHO "                           choosing."
            $ECHO ""
            $ECHO "-p | --path DIR            Requires an ARGUMENT, which is the path to where "
            $ECHO "                           the backups are stored, can be used"
            $ECHO "                           when the default path /sdcard/nandroid/$DEVICEID "
            $ECHO "                           needs to be changed"
            $ECHO ""
            $ECHO "-b | --backup              Will store a full system backup on $BACKUPPATH"
            $ECHO "                           One can suppress backup of any image however with options"
            $ECHO "                           starting with --no[partionname]"
            $ECHO ""
            $ECHO "-e | --ext                 Preserve the contents of the ext partition along with"
            $ECHO "                           the other partitions being backed up, to easily switch roms."
            $ECHO ""
            $ECHO "-a | --android_secure      Preserve the contents of /sdcard/.android_secure along with"
            $ECHO "                           the other partitions being backed up, to easily switch roms."
            $ECHO ""
            $ECHO "-r | --restore             Will restore the last made backup which matches --subname"
            $ECHO "                           ARGUMENT for boot, system, recovery and data"
            $ECHO "                           unless suppressed by other options"
            $ECHO "                           if no --subname is supplied and the user fails to"
            $ECHO "                           provide any input then the latest backup is used"
            $ECHO "                           When restoring compressed backups, the images will remain"
            $ECHO "                           decompressed after the restore, you need to use -c|-compress"
            $ECHO "                           or --bzip2 to compress the backup again"
            $ECHO ""
            $ECHO "-d | --delete              Will remove backups whose names match --subname ARGUMENT"
            $ECHO "                           Will allow to narrow down, will ask if the user is certain."
            $ECHO "                           Removes one backup at a time, repeat to remove multiple backups"
            $ECHO ""
            $ECHO "-c | --compress            Will operate on chosen backups to compress image files,"
            $ECHO "                           resulting in saving of about 40MB out of 90+mb,"
            $ECHO "                           i.e. up to 20 backups can be stored in 1 GIG, if this "
            $ECHO "                           option is turned on with --backup, the resulting backup will"
            $ECHO "                           be compressed, no effect if restoring since restore will"
            $ECHO "                           automatically uncompress compressed images if space is available"
            $ECHO ""
            $ECHO "--bzip2: -#                Turns on -c|--compress and uses bzip2 for compression instead"
            $ECHO "                           of gzip, requires an ARG -[1-9] compression level"
            $ECHO ""
            $ECHO "--webget: URL|\"\"         Requires an argument, a valid URL for an *update*.zip file"
            $ECHO "                           If a null string is passed then the default URL is used"
            $ECHO "                           Will also create an update.MD5sum file and update.name with the"
            $ECHO "                           original file name."
            $ECHO ""
            $ECHO "--nameserver: IP addr      Provide the first nameserver IP address, to override the default"
            $ECHO ""
            $ECHO "--nameserver2: IP addr     Provide the second nameserver IP address, to override the default"
            $ECHO ""
            $ECHO "--webgettarget: DIR        Target directory to deposit the fetched update, default is"
            $ECHO "                           /sdcard"
            $ECHO ""
            $ECHO "--norecovery               Will suppress restore/backup of the recovery partition"
            $ECHO "                           If recovery.img was not part of the backup, no need to use this"
            $ECHO "                           option as the nandroid will not attempt to restore it, same goes"
            $ECHO "                           for all the options below"
            $ECHO ""
            $ECHO "--noboot                   Will suppress restore/backup of the boot partition"
            $ECHO ""
            $ECHO "--nodata                   Will suppress restore/backup of the data partition"
            $ECHO ""
            $ECHO "--nosystem                 Will suppress restore/backup of the system partition"
            $ECHO ""
            $ECHO "--nocache                  Will suppress restore/backup of the cache partition"
            $ECHO ""
            $ECHO "--nomisc                   Will suppress restore/backup of the misc partition"
            $ECHO ""
            $ECHO "--nosplash1                Will suppress restore/backup of the splash1 partition"
            $ECHO ""
            $ECHO "--nosplash2                Will suppress restore/backup of the splash2 partition"
            $ECHO ""
            $ECHO "--nowimax                  Will suppress restore/backup of the wimax partition"
            $ECHO ""
            $ECHO "--defaultinput             Makes nandroid-mobile non-interactive, assumes default"
            $ECHO "                           inputs from the user."
            $ECHO ""
            $ECHO "--autoreboot               Automatically reboot the phone after a backup, especially"
            $ECHO "                           useful when the compression options are on -c|--compress| "
            $ECHO "                           --bzip2 -level since the compression op takes time and"
            $ECHO "                           you may want to go to sleep or do something else, and"
            $ECHO "                           when a backup is over you want the calls and mms coming"
            $ECHO "                           through, right?"
            $ECHO ""
            $ECHO "--autoapplyupdate          Once the specific update is downloaded or chosen from the"
            $ECHO "                           sdcard, apply it immediately. This option is valid as a "
            $ECHO "                           modifier for either --webget or --getupdate options."
            $ECHO ""
            $ECHO "--save: ROMTAG             Preserve EVERYTHING under ROMTAG name, a composite option,"
            $ECHO "                           it uses several other options."
            $ECHO ""
            $ECHO "--switchto: ROMTAG         Restores your entire environment including app2sd apps, cache"
            $ECHO "                           to the ROM environment named ROMTAG."
            $ECHO ""
            $ECHO "-q|--quiet|--silent        Direct all the output to the recovery log, and assume default"
            $ECHO "                           user inputs."
            $ECHO ""
            $ECHO "-l|--listbackup            Will search the entire SDCARD for backup folders and will dump"
            $ECHO "                           the list to stdout for use by the UI. Should be run with --silent"
            $ECHO ""
            $ECHO "--listupdate               Will search the entire SDCARD for updates and will dump"
            $ECHO "                           the list to stdout for use by the UI. Should be run with --silent"
            exit 0
            ;;
        --norecovery)
            NORECOVERY=1
            #$ECHO "No recovery"
            shift
            ;;
        --noboot)
            NOBOOT=1
            #$ECHO "No boot"
            shift
            ;;
        --nodata)
            NODATA=1
            #$ECHO "No data"
            shift
            ;;
        --nosystem)
            NOSYSTEM=1
            #$ECHO "No system"
            shift
            ;;
        --nocache)
            NOCACHE=1
            #$ECHO "No cache"
            shift
            ;;
        --nomisc)
            NOMISC=1
            #$ECHO "No misc"
            shift
            ;;
        --nosplash1)
            NOSPLASH1=1
            #$ECHO "No splash1"
            shift
            ;;
        --nosplash2)
            NOSPLASH2=1
            #$ECHO "No splash2"
            shift
            ;;
        --nowimax)
            NOWIMAX=1
            #$ECHO "No wimax"
            shift
            ;;
        --backup)
            BACKUP=1
            #$ECHO "backup"
            shift
            ;;
        -b)
            BACKUP=1
            #$ECHO "backup"
            shift
            ;;
        -e)
            EXT=1
            shift
            ;;
        --ext)
            EXT=1
            shift
            ;;
        -a)
            ANDROID_SECURE=1
            shift
            ;;
        --android_secure)
            ANDROID_SECURE=1
            shift
            ;;
        --restore)
            RESTORE=1
            #$ECHO "restore"
            shift
            ;;
        -r)
            RESTORE=1
            #$ECHO "restore"
            shift
            ;;
        --compress)
            COMPRESS=1
            #$ECHO "Compress"
            shift
            ;;
        -c)
            COMPRESS=1
            #$ECHO "Compress"
            shift
            ;;
        --bzip2)
            COMPRESS=1
            DEFAULTCOMPRESSOR=bzip2
            DEFAULTEXT=.bz2
            if [ "$2" == "$option" ]; then
                shift
            fi
            DEFAULTLEVEL="$2"
            shift
            ;;
        --getupdate)
            GETUPDATE=1
            shift
            ;;
        -u)
            GETUPDATE=1
            shift
            ;;
        --subname)
            if [ "$2" == "$option" ]; then
                shift
            fi
            #$ECHO $2
            SUBNAME="$2"
            shift
            ;;
        -s)
            if [ "$2" == "$option" ]; then
                shift
            fi
            #$ECHO $2
            SUBNAME="$2"
            shift
            ;;
        --path)
            if [ "$2" == "$option" ]; then
                shift
            fi
            #$ECHO $2
            BACKUPPATH="$2"
            DEFAULTUPDATEPATH="$2"
            shift 2
            ;;
        -p)
            if [ "$2" == "$option" ]; then
                shift
            fi
            #$ECHO $2
            BACKUPPATH="$2"
            shift 2
            ;;
        --delete)
            DELETE=1
            shift
            ;;
        -d)
            DELETE=1
            shift
            ;;
        --webgettarget)
            if [ "$2" == "$option" ]; then
                shift
            fi
            WEBGETTARGET="$2"
            shift
            ;;
        --webget)
            if [ "$2" == "$option" ]; then
                shift
            fi
            #$ECHO "WEBGET"
            # if the argument is "" stick with the default: /sdcard
            if [ ! "$2" == "" ]; then
                WEBGETSOURCE="$2"
            fi
            WEBGET=1
            shift
            ;;
        --nameserver)
            if [ "$2" == "$option" ]; then
                shift
            fi
            NAMESERVER1="$2"
            shift
            ;;
        --nameserver2)
            if [ "$2" == "$option" ]; then
                shift
            fi
            NAMESERVER2="$2"
            shift
            ;;
        --defaultinput)
            ASSUMEDEFAULTUSERINPUT=1
            shift
            ;;
        --autoreboot)
            AUTOREBOOT=1
            shift
            ;;
        --autoapplyupdate)
            AUTOAPPLY=1
            shift
            ;;
        --save)
            if [ "$2" == "$option" ]; then
                shift
            fi
            SUBNAME="$2"
            BACKUP=1
            ASSUMEDEFAULTUSERINPUT=1
            EXT=1
            COMPRESS=1
            shift
            ;;
        --switchto)
            if [ "$2" == "$option" ]; then
                shift
            fi
            SUBNAME="$2"
            RESTORE=1
            ASSUMEDEFAULTUSERINPUT=1
            EXT=1
            shift
            ;;
        -l)
            shift
            LISTBACKUP=1
            ;;
        --listbackup)
            shift
            LISTBACKUP=1
            ;;
        --listupdate)
            shift
            LISTUPDATE=1
            ;;
        --)
            shift
            break
            ;;
    esac
done

$ECHO ""
$ECHO "nandroid-mobile v2.2.1"
$ECHO ""

let OPS=$BACKUP
let OPS=$OPS+$RESTORE
let OPS=$OPS+$DELETE
let OPS=$OPS+$WEBGET
let OPS=$OPS+$GETUPDATE
let OPS=$OPS+$LISTBACKUP
let OPS=$OPS+$LISTUPDATE

if [ "$OPS" -ge 2 ]; then
    ECHO=echo
    $ECHO "--backup, --restore, --delete, --webget, --getupdate, --listbackup, --listupdate  are mutually exclusive operations."
    $ECHO "Please, choose one and only one option!"
    $ECHO "Aborting."
    exit 1
fi

let OPS=$OPS+$COMPRESS

if [ "$OPS" -lt 1 ]; then
    ECHO=echo
    $ECHO "Usage: $0 {-b|--backup|-r|--restore|-d|--delete|-u|--getupdate|--webget|-c|--compress|--bzip2 -level|-l|--listbackup|--listupdate} [options]"
    $ECHO "At least one operation must be defined, try $0 --help for more information."
    exit 0
fi


if [ "$LISTBACKUP" == 1 ]; then
    umount /sdcard 2>/dev/null
    mount /sdcard 2>/dev/null
    CHECK=`mount | grep /sdcard`
    if [ "$CHECK" == "" ]; then
        echo "Error: sdcard not mounted, aborting."
        exit 1
    else
        find /sdcard | grep nandroid.md5 | sed s/.gz//g | sed s/.bz2//g | sed s/nandroid.md5//g
        umount /sdcard 2>/dev/null
        exit 0
    fi
fi

if [ "$LISTUPDATE" == 1 ]; then
    umount /sdcard 2>/dev/null
    mount /sdcard 2>/dev/null
    CHECK=`mount | grep /sdcard`
    if [ "$CHECK" == "" ]; then
        echo "Error: sdcard not mounted, aborting."
        exit 1
    else
        find /sdcard | grep .zip
        umount /sdcard 2>/dev/null
        exit 0
    fi
fi

# Make sure it exists
touch /cache/recovery/log


if [ "$AUTOAPPLY" == 1 -a "$WEBGET" == 0 -a "$GETUPDATE" == 0 ]; then
    ECHO=echo
    $ECHO "The --autoapplyupdate option is valid only in conjunction with --webget or --getupdate."
    $ECHO "Aborting."
    exit 1
fi

if [ "$COMPRESS" == 1 ]; then
    $ECHO "Compressing with $DEFAULTCOMPRESSOR, level $DEFAULTLEVEL"
fi

if [ "$WEBGET" == 1 ]; then
    $ECHO "Fetching $WEBGETSOURCE to target folder: $WEBGETTARGET"
fi

if [ ! "$SUBNAME" == "" ]; then
    if [ "$BACKUP" == 1 ]; then
        if [ "$COMPRESS" == 1 ]; then
            $ECHO "Using $SUBNAME- prefix to create a compressed backup folder"
        else
            $ECHO "Using $SUBNAME- prefix to create a backup folder"
        fi
    else
        if [ "$RESTORE" == 1 -o "$DELETE" == 1 -o "$COMPRESS" == 1 ]; then
            $ECHO "Searching for backup directories, matching $SUBNAME, to delete or restore"
            $ECHO "or compress"
            $ECHO ""
        fi
    fi
else
    if [ "$BACKUP" == 1 ]; then
        $ECHO "Using G1 keyboard, enter a prefix substring and then <CR>"
        $ECHO -n "or just <CR> to accept default: "
        if [ "$ASSUMEDEFAULTUSERINPUT" == 0 ]; then
            read SUBNAME
        else
            $ECHO "Accepting default."
        fi
        $ECHO ""
        if [ "$COMPRESS" == 1 ]; then
            $ECHO "Using $SUBNAME- prefix to create a compressed backup folder"
        else
            $ECHO "Using $SUBNAME- prefix to create a backup folder"
        fi
        $ECHO ""
    else
        if [ "$RESTORE" == 1 -o "$DELETE" == 1 -o "$COMPRESS" == 1 ]; then
            $ECHO "Using G1 keyboard, enter a directory name substring and then <CR>"
            $ECHO -n "to find matches or just <CR> to accept default: "
            if [ "$ASSUMEDEFAULTUSERINPUT" == 0 ]; then
                read SUBNAME
            else
                $ECHO "Accepting default."
            fi
            $ECHO ""
            $ECHO "Using $SUBNAME string to search for matching backup directories"
            $ECHO ""
        fi
    fi
fi

if [ "$BACKUP" == 1 ]; then
		mkyaffs2image=`which mkyaffs2image`
		if [ "$mkyaffs2image" == "" ]; then
			mkyaffs2image=`which mkyaffs2image`
			if [ "$mkyaffs2image" == "" ]; then
				$ECHO "error: mkyaffs2image not found in path"
				exit 1
			fi
		fi
		dump_image=`which dump_image`
		if [ "$dump_image" == "" ]; then
			dump_image=`which dump_image`
			if [ "$dump_image" == "" ]; then
				$ECHO "error: dump_image or dump_image not found in path"
				exit 1
			fi
		fi
fi

if [ "$RESTORE" == 1 ]; then
		flash_image=`which flash_image`
		if [ "$flash_image" == "" ]; then
			flash_image=`which flash_image`
			if [ "$flash_image" == "" ]; then
				$ECHO "error: flash_image or flash_image not found in path"
				exit 1
			fi
		fi
		unyaffs=`which unyaffs`
		if [ "$unyaffs" == "" ]; then
			unyaffs=`which unyaffs`
			if [ "$unyaffs" == "" ]; then
				$ECHO "error: unyaffs not found in path"
				exit 1
			fi
		fi
fi
if [ "$COMPRESS" == 1 ]; then
                compressor=`busybox | grep $DEFAULTCOMPRESSOR`
                if [ "$compressor" == "" ]; then
                    $ECHO "Warning: busybox/$DEFAULTCOMPRESSOR is not found"
                    $ECHO "No compression operations will be performed"
                    COMPRESS=0
                else
                    $ECHO "Found $DEFAULTCOMPRESSOR, will compress the backup"
                fi
fi

# 1
DEVICEID=`cat /proc/cmdline | sed "s/.*serialno=//" | cut -d" " -f1`
RECOVERY=`cat /proc/cmdline | grep "androidboot.mode=recovery"`
if [ "$RECOVERY" == "foo" ]; then
	$ECHO "Error: Must be in recovery mode, aborting"
	exit 1
fi
if [ "$DEVICEID" == "foo" ]; then
	$ECHO "Error: device id not found in /proc/cmdline, aborting"
	exit 1
fi
if [ ! "`id -u 2>/dev/null`" == "0" ]; then
	if [ "`whoami 2>&1 | grep 'uid 0'`" == "" ]; then
		$ECHO "Error: must run as root, aborting"
		exit 1
	fi
fi


if [ "$RESTORE" == 1 ]; then

                umount /sdcard 2>/dev/null
		mount /sdcard 2>/dev/null
		if [ "`mount | grep sdcard`" == "" ]; then
			$ECHO "error: unable to mount /sdcard, aborting"
			exit 1
		fi

		# find the latest backup, but show the user other options
                $ECHO ""
                $ECHO "Looking for the latest backup, will display other choices!"
                $ECHO ""

		RESTOREPATH=`ls -trd $BACKUPPATH/*$SUBNAME* 2>/dev/null | tail -1`
                $ECHO " "

		if [ "$RESTOREPATH" = "" ];
		then
			$ECHO "Error: no backups found"
			exit 2
		else
                        $ECHO "Default backup is the latest: $RESTOREPATH"
                        $ECHO ""
                        $ECHO "Other available backups are: "
                        $ECHO ""
                        ls -trd $BACKUPPATH/*$SUBNAME* 2>/dev/null | grep -v $RESTOREPATH $OUTPUT
                        $ECHO ""
                        $ECHO "Using G1 keyboard, enter a unique name substring to change it and <CR>"
                        $ECHO -n "or just <CR> to accept: "
                        if [ "$ASSUMEDEFAULTUSERINPUT" == 0 ]; then
                            read SUBSTRING
                        else
                            $ECHO "Accepting default."
                            SUBSTRING=""
                        fi
                        $ECHO ""

                        if [ ! "$SUBSTRING" == "" ]; then
                            RESTOREPATH=`ls -trd $BACKUPPATH/*$SUBNAME* 2>/dev/null | grep $SUBSTRING | tail -1`
                        else
                            RESTOREPATH=`ls -trd $BACKUPPATH/*$SUBNAME* 2>/dev/null | tail -1`
                        fi
                        if [ "$RESTOREPATH" = "" ]; then
                               $ECHO "Error: no matching backups found, aborting"
                               exit 2
                        fi
		fi
		
		$ECHO "Restore path: $RESTOREPATH"
                $ECHO ""

                umount /system /data 2>/dev/null
		mount /system 2>/dev/null
		mount /data 2>/dev/null
		if [ "`mount | grep data`" == "" ]; then
			$ECHO "error: unable to mount /data, aborting"	
			exit 1
		fi
		if [ "`mount | grep system`" == "" ]; then
			$ECHO "error: unable to mount /system, aborting"	
			exit 1
		fi
		
		CWD=$PWD
		cd $RESTOREPATH

                DEFAULTEXT=""
                if [ `ls *.bz2 2>/dev/null|wc -l` -ge 1 ]; then
                    DEFAULTCOMPRESSOR=bzip2
                    DEFAULTDECOMPRESSOR=bunzip2
                    DEFAULTEXT=.bz2
                fi
                if [ `ls *.gz 2>/dev/null|wc -l` -ge 1 ]; then
                    DEFAULTCOMPRESSOR=gzip
                    DEFAULTDECOMPRESSOR=gunzip
                    DEFAULTEXT=.gz
                fi

		if [ ! -f $RESTOREPATH/nandroid.md5$DEFAULTEXT ]; then
			$ECHO "error: $RESTOREPATH/nandroid.md5 not found, cannot verify backup data"
			exit 1
		fi

                if [ `ls *.bz2 2>/dev/null|wc -l` -ge 1 -o `ls *.gz 2>/dev/null|wc -l` -ge 1 ]; then
                    $ECHO "This backup is compressed with $DEFAULTCOMPRESSOR."

                    # Make sure that $DEFAULT[DE]COMPRESSOR exists
                    if [ `busybox | grep $DEFAULTCOMPRESSOR | wc -l` -le 0 -a\
                            `busybox | grep $DEFAULTDECOMPRESSOR | wc -l` -le 0 ]; then

                        $ECHO "You do not have either the $DEFAULTCOMPRESSOR or the $DEFAULTDECOMPRESSOR"
                        $ECHO "to unpack this backup, cleaning up and aborting!"
                        umount /system 2>/dev/null
                        umount /data 2>/dev/null
                        umount /sdcard 2>/dev/null
                        exit 1
                    fi
                    $ECHO "Checking free space /sdcard for the decompression operation."
                    FREEBLOCKS="`df -k /sdcard| grep sdcard | awk '{ print $4 }'`"
                    # we need about 100MB for gzip to uncompress the files
                    if [ $FREEBLOCKS -le 100000 ]; then
                        $ECHO "Error: not enough free space available on sdcard (need about 100mb)"
                        $ECHO "to perform restore from the compressed images, aborting."
                        umount /system 2>/dev/null
                        umount /data 2>/dev/null
                        umount /sdcard 2>/dev/null
                        exit 1
                    fi
                    $ECHO "Decompressing images, please wait...."
                    $ECHO ""
                    # Starting from the largest while we still have more space to reduce
                    # space requirements
                    $DEFAULTCOMPRESSOR -d `ls -S *$DEFAULTEXT`
                    $ECHO "Backup images decompressed"
                    $ECHO ""
                fi

		$ECHO "Verifying backup images..."
		md5sum -c nandroid.md5
		if [ $? -eq 1 ]; then
			$ECHO "Error: md5sum mismatch, aborting"
			exit 1
		fi

                if [ `ls boot* 2>/dev/null | wc -l` == 0 ]; then
                    NOBOOT=1
                fi
                if [ `ls recovery* 2>/dev/null | wc -l` == 0 ]; then
                    NORECOVERY=1
                fi
                if [ `ls data* 2>/dev/null | wc -l` == 0 ]; then
                    NODATA=1
                fi
                if [ `ls system* 2>/dev/null | wc -l` == 0 ]; then
                    NOSYSTEM=1
                fi
		# Amon_RA : If there's no wimax backup set nowimax to 1 so wimax restore doesn't start                
                if [ `ls wimax* 2>/dev/null | wc -l` == 0 ]; then
                    NOWIMAX=1
                fi
		# Amon_RA : If there's no ext backup set ext to 0 so ext restore doesn't start                
		if [ `ls ext* 2>/dev/null | wc -l` == 0 ]; then
                    EXT=0
                fi
		# Amon_RA : If there's no android_secure backup set android_secure to 0 so android_secure restore doesn't start                
		if [ `ls android_secure* 2>/dev/null | wc -l` == 0 ]; then
                    ANDROID_SECURE=0
                fi

		for image in boot recovery wimax; do
                    if [ "$NOBOOT" == "1" -a "$image" == "boot" ]; then
                        $ECHO ""
                        $ECHO "Not flashing boot image!"
                        $ECHO ""
                        continue
                    fi
                    if [ "$NORECOVERY" == "1" -a "$image" == "recovery" ]; then
                        $ECHO ""
                        $ECHO "Not flashing recovery image!"
                        $ECHO ""
                        continue
                    fi
                    if [ "$NOWIMAX" == "1" -a "$image" == "wimax" ]; then
                        $ECHO ""
                        $ECHO "Not flashing wimax image!"
                        $ECHO ""
                        continue
                    fi
                    $ECHO "Flashing $image..."
		    $flash_image $image $image.img $OUTPUT
                done

		for image in data system; do
                        if [ "$NODATA" == "1" -a "$image" == "data" ]; then
                            $ECHO ""
                            $ECHO "Not restoring data image!"
                            $ECHO ""
                            continue
                        fi
                        if [ "$NOSYSTEM" == "1" -a "$image" == "system" ]; then
                            $ECHO ""
                            $ECHO "Not restoring system image!"
                            $ECHO ""
                            continue
                        fi
			$ECHO "Erasing /$image..."
			cd /$image
			rm -rf * 2>/dev/null
			$ECHO "Unpacking $image image..."
			$unyaffs $RESTOREPATH/$image.img $OUTPUT
			cd /
			sync
			umount /$image
		done

                if [ "$EXT" == 1 ]; then
			# Amon_RA : Check if there's an ext partition before starting to restore    		
			if [ -e /dev/block/mmcblk0p2 ]; then
	                    $ECHO "Restoring the ext contents."
	                    CWD=`pwd`
	                    cd /

	                    if [ `mount | grep /sd-ext | wc -l` == 0 ]; then
	                        mount /sd-ext
	                    fi

	                    cd $CWD
	                    CHECK=`mount | grep /sd-ext`

	                    if [ "$CHECK" == "" ]; then
	                        $ECHO "Warning: --ext specified but unable to mount the ext partition."
	                        $ECHO "Warning: your phone may be in an inconsistent state on reboot."
	                        exit 1
	                    else
	                        CWD=`pwd`
	                        cd /sd-ext
	                        # Depending on whether the ext backup is compressed we do either or.
	                        if [ -e $RESTOREPATH/ext.tar ]; then 
	                            rm -rf ./* 2>/dev/null
	                            tar -x$TARFLAGS -f $RESTOREPATH/ext.tar
	                        else
	                            if [ -e $RESTOREPATH/ext.tgz ]; then
	                                rm -rf ./* 2>/dev/null
	                                tar -x$TARFLAGS -zf $RESTOREPATH/ext.tgz
	                            else
	                                if [ -e $RESTOREPATH/ext.tar.bz2 ]; then
	                                    rm -rf ./* 2>/dev/null
	                                    tar -x$TARFLAGS -jf $RESTOREPATH/ext.tar.bz2
	                                else
	                                    $ECHO "Warning: --ext specified but cannot find the ext backup."
	                                    $ECHO "Warning: your phone may be in an inconsistent state on reboot."
	                                fi
	                            fi
	                        fi
	                        cd $CWD
	                        sync
	                        umount /sd-ext
	
	                    fi
			else
	                        # Amon_RA : Just display a warning
				$ECHO "Warning: --ext specified but ext partition present on sdcard"
	                        $ECHO "Warning: your phone may be in an inconsistent state on reboot."     
                	fi
		fi

                if [ "$ANDROID_SECURE" == 1 ]; then

	                        CWD=`pwd`
	                        cd /sdcard
	                        # Depending on whether the android_secure backup is compressed we do either or.
	                        if [ -e $RESTOREPATH/android_secure.tar ]; then 
	                            rm -rf .android_secure 2>/dev/null
	                            tar -x$TARFLAGS -f $RESTOREPATH/android_secure.tar
	                        else
	                            if [ -e $RESTOREPATH/android_secure.tgz ]; then
	                                rm -rf .android_secure 2>/dev/null
	                                tar -x$TARFLAGS -zf $RESTOREPATH/android_secure.tgz
	                            else
	                                if [ -e $RESTOREPATH/android_secure.tar.bz2 ]; then
	                                    rm -rf .android_secure 2>/dev/null
	                                    tar -x$TARFLAGS -jf $RESTOREPATH/android_secure.tar.bz2
	                                else
	                                    $ECHO "Warning: --android_secure specified but cannot find the android_secure backup."
	                                    $ECHO "Warning: your phone may be in an inconsistent state on reboot."
	                                fi
	                            fi
	                        fi
	                        cd $CWD
	                        sync
		fi


		$ECHO "Restore done"
		exit 0
fi

# 2.
if [ "$BACKUP" == 1 ]; then

$ECHO "mounting system and data read-only, sdcard read-write"
umount /system 2>/dev/null
umount /data 2>/dev/null
umount /sdcard 2>/dev/null
mount -o ro /system || FAIL=1
mount -o ro /data || FAIL=2
mount /sdcard || mount /dev/block/mmcblk0 /sdcard || FAIL=3
case $FAIL in
	1) $ECHO "Error mounting system read-only"; umount /system /data /sdcard; exit 1;;
	2) $ECHO "Error mounting data read-only"; umount /system /data /sdcard; exit 1;;
	3) $ECHO "Error mounting sdcard read-write"; umount /system /data /sdcard; exit 1;;
esac

if [ ! "$SUBNAME" == "" ]; then
    SUBNAME=$SUBNAME-
fi

# Identify the backup with what partitions have been backed up
if [ "$NOBOOT" == 0 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"B"
fi
if [ "$NOCACHE" == 0 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"C"
fi
if [ "$NODATA" == 0 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"D"
fi
if [ "$EXT" == 1 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"E"
fi
if [ "$ANDROID_SECURE" == 1 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"A"
fi
if [ "$NOMISC" == 0 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"M"
fi
if [ "$NORECOVERY" == 0 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"R"
fi
if [ "$NOSYSTEM" == 0 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"S"
fi

if [ ! -e /dev/mtd/mtd6ro ]; then
    NOSPLASH1=1
    NOSPLASH2=1
fi

if [ "$NOSPLASH1" == 0 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"1"
fi
if [ "$NOSPLASH2" == 0 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"2"
fi

if [ "$NOWIMAX" == 0 ]; then
    BACKUPLEGEND=$BACKUPLEGEND"W"
fi

if [ ! "$BACKUPLEGEND" == "" ]; then
    BACKUPLEGEND=$BACKUPLEGEND-
fi


TIMESTAMP="`date +%Y%m%d-%H%M`"
DESTDIR="$BACKUPPATH/$SUBNAME$BACKUPLEGEND$TIMESTAMP"
if [ ! -d $DESTDIR ]; then 
	mkdir -p $DESTDIR
	if [ ! -d $DESTDIR ]; then 
		$ECHO "error: cannot create $DESTDIR"
		umount /system 2>/dev/null
		umount /data 2>/dev/null
		umount /sdcard 2>/dev/null
		exit 1
	fi
else
	touch $DESTDIR/.nandroidwritable
	if [ ! -e $DESTDIR/.nandroidwritable ]; then
		$ECHO "error: cannot write to $DESTDIR"
		umount /system 2>/dev/null
		umount /data 2>/dev/null
		umount /sdcard 2>/dev/null
		exit 1
	fi
	rm $DESTDIR/.nandroidwritable
fi

# 3.
$ECHO "checking free space on sdcard"
FREEBLOCKS="`df -k /sdcard| grep sdcard | awk '{ print $4 }'`"
# we need about 500MB for the dump
if [ $FREEBLOCKS -le 500000 ]; then
	$ECHO "Error: not enough free space available on sdcard (need 500mb), aborting."
	umount /system 2>/dev/null
	umount /data 2>/dev/null
	umount /sdcard 2>/dev/null
	exit 1
fi



if [ -e /dev/mtd/mtd6ro ]; then
    if [ "$NOSPLASH1" == 0 ]; then
	$ECHO -n "Dumping splash1 from device over tcp to $DESTDIR/splash1.img..."
	dd if=/dev/mtd/mtd6ro of=$DESTDIR/splash1.img skip=19072 bs=2048 count=150 2>/dev/null
	$ECHO "done"
	sleep 1s
    else
        $ECHO "Dump of the splash1 image suppressed."
    fi
    if [ "$NOSPLASH2" == 0 ]; then
	$ECHO -n "Dumping splash2 from device over tcp to $DESTDIR/splash2.img..."
	dd if=/dev/mtd/mtd6ro of=$DESTDIR/splash2.img skip=19456 bs=2048 count=150 2>/dev/null
	$ECHO "done"
    else
        $ECHO "Dump of the splash2 image suppressed."
    fi
fi


# 5.
for image in boot recovery misc wimax; do

    case $image in
        boot)
            if [ "$NOBOOT" == 1 ]; then
                $ECHO "Dump of the boot partition suppressed."
                continue
            fi
            ;;
        recovery)
            if [ "$NORECOVERY" == 1 ]; then
                $ECHO "Dump of the recovery partition suppressed."
                continue
            fi
            ;;
        misc)
            if [ "$NOMISC" == 1 ]; then
                $ECHO "Dump of the misc partition suppressed."
                continue
            fi
            ;;
        wimax)
            if [ "$NOWIMAX" == 1 ]; then
                $ECHO "Dump of the wimax partition suppressed."
                continue
            fi
            ;;

    esac

	# 5a
	DEVICEMD5=`$dump_image $image - | md5sum | awk '{ print $1 }'`
	sleep 1s
	MD5RESULT=1
	# 5b
	$ECHO -n "Dumping $image to $DESTDIR/$image.img..."
	ATTEMPT=0
	while [ $MD5RESULT -eq 1 ]; do
		let ATTEMPT=$ATTEMPT+1
		# 5b1
		$dump_image $image $DESTDIR/$image.img $OUTPUT
		sync
		# 5b3
		echo "${DEVICEMD5}  $DESTDIR/$image.img" | md5sum -c -s - $OUTPUT
		if [ $? -eq 1 ]; then
			true
		else
			MD5RESULT=0
		fi
		if [ "$ATTEMPT" == "5" ]; then
			$ECHO "Fatal error while trying to dump $image, aborting."
			umount /system
			umount /data
			umount /sdcard
			exit 1
		fi
	done
	$ECHO "done"
done

# 6
for image in system data cache; do
    case $image in
        system)
            if [ "$NOSYSTEM" == 1 ]; then
                $ECHO "Dump of the system partition suppressed."
                continue
            fi
            ;;
        data)
            if [ "$NODATA" == 1 ]; then
                $ECHO "Dump of the data partition suppressed."
                continue
            fi
            ;;
        cache)
            if [ "$NOCACHE" == 1 ]; then
                $ECHO "Dump of the cache partition suppressed."
                continue
            fi
            ;;
    esac

	# 6a
	$ECHO -n "Dumping $image to $DESTDIR/$image.img..."
	$mkyaffs2image /$image $DESTDIR/$image.img $OUTPUT
	sync
	$ECHO "done"
done

# Backing up the ext partition, not really for the backup but to switch ROMS and apps at the same time.

if [ "$EXT" == 1 ]; then
    $ECHO "Storing the ext contents in the backup folder."

    CHECK=`mount | grep /sd-ext`
    if [ "$CHECK" == "" ]; then
        mount /sd-ext 2>/dev/null
    fi
    
    CHECK=`mount | grep /sd-ext`
    if [ "$CHECK" == "" ]; then
          $ECHO "Warning: --ext specified but unable to mount the ext partition."
          $ECHO "Skipping /sd-ext backup."
    else
        
        CWD=`pwd`
        cd /sd-ext
        # Depending on the whether we want it compressed we do either or.
        if [ "$COMPRESS" == 0 ]; then 
            tar -cvf $DESTDIR/ext.tar ./*
        else
            if [ "$DEFAULTCOMPRESSOR" == "bzip2" ]; then
                tar -cvjf $DESTDIR/ext.tar.bz2 ./*
            else
                tar -cvzf $DESTDIR/ext.tgz ./*
            fi
        fi
        cd $CWD
        umount /sd-ext
    fi
fi

# Backing up the /sdcard/.android_secure, not really for the backup but to switch ROMS and apps at the same time.

if [ "$ANDROID_SECURE" == 1 ]; then
    $ECHO "Storing the /sdcard/.android_secure contents in the backup folder."      
        CWD=`pwd`
        cd /sdcard
        # Depending on the whether we want it compressed we do either or.
        if [ "$COMPRESS" == 0 ]; then 
            tar -cvf $DESTDIR/android_secure.tar ./.android_secure*
        else
            if [ "$DEFAULTCOMPRESSOR" == "bzip2" ]; then
                tar -cvjf $DESTDIR/android_sercure.tar.bz2 ./.android_secure*
            else
                tar -cvzf $DESTDIR/android_secure.tgz ./.android_secure*
            fi
        fi
        cd $CWD
fi

# 7.
$ECHO -n "generating md5sum file..."
CWD=$PWD
cd $DESTDIR
md5sum *img > nandroid.md5

# 7b.
if [ "$COMPRESS" == 1 ]; then
    $ECHO "Compressing the backup, may take a bit of time, please wait..."
    $ECHO "checking free space on sdcard for the compression operation."
    FREEBLOCKS="`df -k /sdcard| grep sdcard | awk '{ print $4 }'`"
    # we need about 70MB for the intermediate storage needs
    if [ $FREEBLOCKS -le 70000 ]; then
	$ECHO "error: not enough free space available on sdcard for compression operation (need 70mb)"
        $ECHO "leaving this backup uncompressed."
    else
        # we are already in $DESTDIR, start compression from the smallest files
        # to maximize space for the largest's compression, less likely to fail.
        # To decompress reverse the order.
        $DEFAULTCOMPRESSOR $DEFAULTLEVEL `ls -S -r * | grep -v ext`
    fi
fi

cd $CWD
$ECHO "done"

# 8.
$ECHO "unmounting system, data and sdcard"
umount /system
umount /data
umount /sdcard

# 9.
$ECHO "Backup successful."
if [ "$AUTOREBOOT" == 1 ]; then
    reboot
fi
exit 0
fi


# ----------------------------------GETTING UPDATES DIRECT FROM THE WEB USING WIFI-------------

if [ "$WEBGET" == 1 ]; then
    $ECHO "mounting system and data read-only, sdcard read-write"
    umount /system 2>/dev/null
    umount /data 2>/dev/null
    umount /sdcard 2>/dev/null

    # Since we are in recovery, these file-systems have to be mounted
    $ECHO "Mounting /system and /data for starting WiFi"
    mount -o ro /system || FAIL=1
    # Need to write to this system to setup nameservers for the wifi
    mount -o rw /data || FAIL=2
    mount /sdcard || mount /dev/block/mmcblk0 /sdcard || FAIL=3

    case $FAIL in
	1) $ECHO "Error mounting system read-only"; umount /system /data /sdcard; exit 1;;
	2) $ECHO "Error mounting data read-write"; umount /system /data /sdcard; exit 1;;
	3) $ECHO "Error mounting sdcard read-write"; umount /system /data /sdcard; exit 1;;
    esac

    if [ "$WEBGETSOURCE" == "" ]; then 
        # Set the URL to the current latest update
        if [ "$ITSANUPDATE" == 1 ]; then
            WEBGETSOURCE=$DEFAULTWEBUPDATE
        else
            WEBGETSOURCE=$DEFAULTWEBIMAGE
        fi
    fi

    if [ "$AUTOAPPLY" == 0 ]; then
        # Need to check space on sdcard only if we dump the update there.
        $ECHO "Checking free space on sdcard for the update download."
        FREEBLOCKS="`df -k /sdcard| grep sdcard | awk '{ print $4 }'`"
        # we need about 50MB for the storage needs
        if [ $FREEBLOCKS -le 50000 ]; then
            $ECHO "Error: not enough free space available on sdcard for the update operation (need 50mb)"
            $ECHO "Please free up space before invoking this option again."
            $ECHO "Cleaning up, unmounting file systems, aborting."
            umount /system /data /sdcard
            exit 1
        fi
    fi

    if [ ! `basename $WEBGETSOURCE` == `basename $WEBGETSOURCE .zip` ]; then
        # It is a zip, not img.
        ITSANUPDATE=1
    else
        if [ ! `basename $WEBGETSOURCE` == `basename $WEBGETSOURCE .img` ]; then
           # It is an img file.
           ITSANIMAGE=1
        else
            # Unknown file type
            $ECHO "Unknown file type, cleaning up, aborting."
            umount /system /data /sdcard
            exit 1
        fi
    fi


    if [ "$ITSANUPDATE" == 1 -a "$AUTOAPPLY" == 0 ]; then
         # Move the previous update aside, if things go badly with the new update, it is good
         # to have the last one still around :-)

         # If we cannot figure out what the file name used to be, create this new one with a time stamp
        OLDNAME="OLD-update-`date +%Y%m%d-%H%M`"

        if [ -e $WEBGETTARGET/update.zip ]; then
            $ECHO "There is already an update.zip in $WEBGETTARGET, backing it up to"
            if [ -e $WEBGETTARGET/update.name ]; then
                OLDNAME=`cat $WEBGETTARGET/update.name`
                # Backup the name file (presumably contains the old name of the update.zip
                mv -f $WEBGETTARGET/update.name $WEBGETTARGET/`basename $OLDNAME .zip`.name
            fi
            $ECHO "`basename $OLDNAME .zip`.zip"
            mv -f $WEBGETTARGET/update.zip $WEBGETTARGET/`basename $OLDNAME .zip`.zip

            # Backup the MD5sum file
            if [ -e $WEBGETTARGET/update.MD5sum ]; then
                mv -f $WEBGETTARGET/update.MD5sum $WEBGETTARGET/`basename $OLDNAME .zip`.MD5sum
            fi
        fi
    fi

    $ECHO "Starting WiFI, please wait..."
    insmod /system/lib/modules/wlan.ko

    wlan_loader -f /system/etc/wifi/Fw1251r1c.bin -e /proc/calibration -i /system/etc/wifi/tiwlan.ini

    CWD=`pwd`
    cd /data/local/tmp

    wpa_supplicant -f -Dtiwlan0 -itiwlan0 -c/data/misc/wifi/wpa_supplicant.conf&

    sleep 5
    $ECHO "wpa_supplicant started"
    $ECHO ""

    echo "nameserver $NAMESERVER1" >/etc/resolv.conf
    echo "nameserver $NAMESERVER2" >>/etc/resolv.conf

    #We want the wifi to assign a dynamic address
    $ECHO "Starting DHCPCD server (dynamic address assignment)"
    # -BKL flags????
    dhcpcd -d tiwlan0 2>/dev/null &

    # Have to wait for it to init stuff
    sleep 10


    CHECK1=`ps | grep -v grep | grep dhcpcd`
    CHECK2=`ps | grep -v grep | grep wpa_supplicant`
    if [ "$CHECK1" == "" -o "$CHECK2" == "" ]; then
         $ECHO "Error: wpa_supplicant or DHCPCD server is not running, cleaning up, aborting"
         rm -- -Dtiwlan0
         cd $CWD

         $ECHO "unmounting /system, /data and /sdcard"
         umount /system
         umount /data
         umount /sdcard
         exit 2
    fi

    $ECHO "DHCPCD server started"
    $ECHO ""

    $ECHO "WiFi is running!"
    $ECHO ""

    if [ "$AUTOAPPLY" == 1 ]; then
        $ECHO "Autoapply is on, retrieving the update into /cache/`basename $WEBGETSOURCE`"

        wget -O /cache/`basename $WEBGETSOURCE` $WEBGETSOURCE $OUTPUT

        if [ ! -e /cache/recovery ]; then
            mkdir /cache/recovery
            chmod 777 /cache/recovery
        fi
        if [ -e /cache/recovery/command ]; then
            echo "--update_package=CACHE:`basename $WEBGETSOURCE`" >>/cache/recovery/command
        else
            echo "--update_package=CACHE:`basename $WEBGETSOURCE`" >/cache/recovery/command
        fi
        chmod 555 /cache/recovery/command
        # Once rebooted the update will be applied.

    else

        if [ "$ITSANUPDATE" == 1 ]; then
            $ECHO "Retrieving system update into $WEBGETTARGET/update.zip, please wait..."
            wget -O $WEBGETTARGET/update.zip $WEBGETSOURCE $OUTPUT

            echo "`basename $WEBGETSOURCE`" > $WEBGETTARGET/update.name
            $ECHO ""
            $ECHO "Update retrieved, if concerned, please compare the md5sum with the number"
            $ECHO "you see on the web page, if it is NOT the same, the retrieval"
            $ECHO "has failed and has to be repeated."
            $ECHO ""
            $ECHO `md5sum $WEBGETTARGET/update.zip | tee $WEBGETTARGET/update.MD5sum`
            $ECHO ""
            $ECHO "MD5sum has been stored in $WEBGETTARGET/update.MD5sum"
        else
            $ECHO "Retrieving the image into $WEBGETTARGET/`basename $WEBGETSOURCE`, please wait..."
            wget -O $WEBGETTARGET/`basename $WEBGETSOURCE` $WEBGETSOURCE $OUTPUT
            $ECHO ""
            $ECHO "$WEBGETSOURCE retrieved, if concerned, please compare the md5sum with the number"
            $ECHO "you see on the web page, if it is NOT the same, the retrieval"
            $ECHO "has failed and has to be repeated."
            $ECHO ""
            md5sum $WEBGETTARGET/`basename $WEBGETSOURCE` | tee $WEBGETTARGET/`basename $WEBGETSOURCE .img`.MD5sum $OUTPUT
            $ECHO ""
            $ECHO "MD5sum has been stored in $WEBGETTARGET/`basename $WEBGETSOURCE .img`.MD5sum"
            $ECHO ""
            $ECHO -n "Would you like to flash this image into boot or recovery? (or no for no flash) "
            read ANSWER
            if [ "$ANSWER" == "boot" ]; then
                 $ECHO "Flashing $WEBGETTARGET/`basename $WEBGETSOURCE` into the boot partition."
                $flash_image boot $WEBGETTARGET/`basename $WEBGETSOURCE`
            else
                if [ "$ANSWER" == "recovery" ]; then
                    $ECHO "Moving $WEBGETTARGET/`basename $WEBGETSOURCE` into the /data/recovery.img"
                    $ECHO "and /system/recovery.img"
                    cp -f $WEBGETTARGET/`basename $WEBGETSOURCE` /data/recovery.img
                    mount -o rw,remount /system
                    cp -f $WEBGETTARGET/`basename $WEBGETSOURCE` /system/recovery.img
                    $ECHO "Depending on the settings of your specific ROM, the recovery.img will be"
                    $ECHO "flashed at the normal bootup time either from /system or /data."
                else
                    $ECHO "Not flashing the image."
                fi
            fi
        fi
        $ECHO ""

    fi

    $ECHO "Shutting down DHCPCD service and wpa_supplicant"
    killall -TERM dhcpcd
    TEMPVAR=`ps | grep -v grep | grep wpa_supplicant`
    TEMPVAR=`echo $TEMPVAR | cut -f 1 -d ' '`
    kill -TERM $TEMPVAR

    while true; do
        CHECK=`ps | grep -v grep | grep dhcpcd`
        if [ ! "$CHECK" == "" ]; then
            sleep 1
        else
            break
        fi
    done

    while true; do
        CHECK=`ps | grep -v grep | grep wpa_supplicant`
        if [ ! "$CHECK" == "" ]; then
            sleep 1
        else
            break
        fi
    done
    #sleep 5

    $ECHO "Cleaning up..."
    # Looks like cannot clean up wlan module since chdir is missing
    #rmmod wlan
    rm -- -Dtiwlan0
    cd $CWD

    $ECHO "unmounting /system, /data and /sdcard"
    umount /system
    umount /data
    umount /sdcard

    if [ "$AUTOAPPLY" == 1 ]; then
        $ECHO "Auto apply update is on, rebooting into recovery to apply the update."
        $ECHO "When the update is complete reboot into the normal mode."
        $ECHO "The device will reboot and the update will be applied in 10 seconds!"
        sleep 10
        reboot recovery
    else
        if [ "$ITSANUPDATE" == 1 ]; then
            $ECHO "If you put the update into a folder other than /sdcard you need to use --getupdate to"
            $ECHO "prepare the update for application."
            $ECHO "You may want to execute 'reboot recovery' and choose update option to flash the update."
            $ECHO "Or in the alternative, shutdown your phone with reboot -p, and then press <CAMERA>+<POWER>"
            $ECHO "to initiate a normal system update procedure, if you have stock SPL."
        fi
        exit 0
    fi
fi

# -------------------------------------DELETION, COMPRESSION OF BACKUPS---------------------------------
if [ "$COMPRESS" == 1 -o "$DELETE" == 1 ]; then
    $ECHO "Unmounting /system and /data to be on the safe side, mounting /sdcard read-write."
    umount /system 2>/dev/null
    umount /data 2>/dev/null
    umount /sdcard 2>/dev/null

    FAIL=0
    # Since we are in recovery, these file-system have to be mounted
    $ECHO "Mounting /sdcard to look for backups."
    mount /sdcard || mount /dev/block/mmcblk0 /sdcard || FAIL=1

    if [ "$FAIL" == 1 ]; then
	$ECHO "Error mounting /sdcard read-write, cleaning up..."; umount /system /data /sdcard; exit 1
    fi

    $ECHO "The current size of /sdcard FAT32 filesystem is `du /sdcard | tail -1 | cut -f 1 -d '/'`Kb"
    $ECHO ""

    # find the oldest backup, but show the user other options
    $ECHO "Looking for the oldest backup to delete, newest to compress,"
    $ECHO "will display all choices!"
    $ECHO ""
    $ECHO "Here are the backups you have picked within this repository $BACKUPPATH:"

    if [ "$DELETE" == 1 ]; then
        RESTOREPATH=`ls -td $BACKUPPATH/*$SUBNAME* 2>/dev/null | tail -1`
        ls -td $BACKUPPATH/*$SUBNAME* 2>/dev/null $OUTPUT
    else
        RESTOREPATH=`ls -trd $BACKUPPATH/*$SUBNAME* 2>/dev/null | tail -1`
        ls -trd $BACKUPPATH/*$SUBNAME* 2>/dev/null $OUTPUT
    fi
    $ECHO " "

    if [ "$RESTOREPATH" = "" ];	then
	$ECHO "Error: no backups found"
	    exit 2
	else
            if [ "$DELETE" == 1 ]; then
                $ECHO "Default backup to delete is the oldest: $RESTOREPATH"
                $ECHO ""
                $ECHO "Other candidates for deletion are: "
                ls -td $BACKUPPATH/*$SUBNAME* 2>/dev/null | grep -v $RESTOREPATH $OUTPUT
            fi
            if [ "$COMPRESS" == 1 ]; then
                $ECHO "Default backup to compress is the latest: $RESTOREPATH"
                $ECHO ""
                $ECHO "Other candidates for compression are: "
                ls -trd $BACKUPPATH/*$SUBNAME* 2>/dev/null | grep -v $RESTOREPATH $OUTPUT
            fi

            $ECHO ""
            $ECHO "Using G1 keyboard, enter a unique name substring to change it and <CR>"
            $ECHO -n "or just <CR> to accept: "
            if [ "$ASSUMEDEFAULTUSERINPUT" == 0 ]; then
                 read SUBSTRING
            else
                $ECHO "Accepting default."
                SUBSTRING=""
            fi

            if [ ! "$SUBSTRING" == "" ]; then
                 RESTOREPATH=`ls -td $BACKUPPATH/*$SUBNAME* 2>/dev/null | grep $SUBSTRING | tail -1`
            else
                 RESTOREPATH=`ls -td $BACKUPPATH/*$SUBNAME* 2>/dev/null | tail -1`
            fi
            if [ "$RESTOREPATH" = "" ]; then
                 $ECHO "Error: no matching backup found, aborting"
                 exit 2
            fi
     fi
		
     if [ "$DELETE" == 1 ]; then
         $ECHO "Deletion path: $RESTOREPATH"
         $ECHO ""
         $ECHO "WARNING: Deletion of a backup is an IRREVERSIBLE action!!!"
         $ECHO -n "Are you absolutely sure? {yes | YES | Yes | no | NO | No}: "
         if [ "$ASSUMEDEFAULTUSERINPUT" == 0 ]; then
             read ANSWER
         else
             ANSWER=yes
             $ECHO "Accepting default."
         fi
         $ECHO ""
         if [ "$ANSWER" == "yes" -o "$ANSWER" == "YES" -o "$ANSWER" == "Yes" ]; then
             rm -rf $RESTOREPATH
             $ECHO ""
             $ECHO "$RESTOREPATH has been permanently removed from your SDCARD."
             $ECHO "Post deletion size of the /sdcard FAT32 filesystem is `du /sdcard | tail -1 | cut -f 1 -d '/'`Kb"
         else 
             if [ "$ANSWER" == "no" -o "$ANSWER" == "NO" -o "$ANSWER" == "No" ]; then
                 $ECHO "The chosen backup will NOT be removed."
             else 
                 $ECHO "Invalid answer: assuming NO."
             fi
         fi
     fi

     if [ "$COMPRESS" == 1 ]; then
         
         CWD=`pwd`
         cd $RESTOREPATH

         if [ `ls *.bz2 2>/dev/null|wc -l` -ge 1 -o `ls *.gz 2>/dev/null|wc -l` -ge 1 ]; then
             $ECHO "This backup is already compressed, cleaning up, aborting..."
             cd $CWD
             umount /sdcard 2>/dev/null
             exit 0
         fi

         $ECHO "checking free space on sdcard for the compression operation."
         FREEBLOCKS="`df -k /sdcard| grep sdcard | awk '{ print $4 }'`"
         # we need about 70MB for the intermediate storage needs
         if [ $FREEBLOCKS -le 70000 ]; then
             $ECHO "Error: not enough free space available on sdcard for compression operation (need 70mb)"
             $ECHO "leaving this backup uncompressed."
         else
             # we are already in $DESTDIR, start compression from the smallest files
             # to maximize space for the largest's compression, less likely to fail.
             # To decompress reverse the order.
             $ECHO "Pre compression size of the /sdcard FAT32 filesystem is `du /sdcard | tail -1 | cut -f 1 -d '/'`Kb"
             $ECHO ""
             $ECHO "Compressing the backup may take a bit of time, please wait..."
             $DEFAULTCOMPRESSOR $DEFAULTLEVEL `ls -S -r *`
             $ECHO ""
             $ECHO "Post compression size of the /sdcard FAT32 filesystem is `du /sdcard | tail -1 | cut -f 1 -d '/'`Kb"
         fi
     fi

     $ECHO "Cleaning up."
     cd $CWD
     umount /sdcard 2>/dev/null
     exit 0

fi

if [ "$GETUPDATE" == 1 ]; then
    $ECHO "Unmounting /system and /data to be on the safe side, mounting /sdcard read-write."
    umount /system 2>/dev/null
    umount /data 2>/dev/null
    umount /sdcard 2>/dev/null

    FAIL=0
    # Since we are in recovery, these file-system have to be mounted
    $ECHO "Mounting /sdcard to look for updates to flash."
    mount /sdcard || mount /dev/block/mmcblk0 /sdcard || FAIL=1

    if [ "$FAIL" == 1 ]; then
	$ECHO "Error mounting /sdcard read-write, cleaning up..."; umount /system /data /sdcard; exit 1
    fi

    $ECHO "The current size of /sdcard FAT32 filesystem is `du /sdcard | tail -1 | cut -f 1 -d '/'`Kb"
    $ECHO ""

    # find all the files with update in them, but show the user other options
    $ECHO "Looking for all *update*.zip candidate files to flash."
    $ECHO ""
    $ECHO "Here are the updates limited by the subname $SUBNAME found"
    $ECHO "within the repository $DEFAULTUPDATEPATH:"
    $ECHO ""
    RESTOREPATH=`ls -trd $DEFAULTUPDATEPATH/*$SUBNAME*.zip 2>/dev/null | grep update | tail -1`
    if [ "$RESTOREPATH" == "" ]; then
        $ECHO "Error: found no matching updates, cleaning up, aborting..."
        umount /sdcard 2>/dev/null
        exit 2
    fi
    ls -trd $DEFAULTUPDATEPATH/*$SUBNAME*.zip 2>/dev/null | grep update $OUTPUT
    $ECHO ""
    $ECHO "The default update is the latest $RESTOREPATH"
    $ECHO ""
    $ECHO "Using G1 keyboard, enter a unique name substring to change it and <CR>"
    $ECHO -n "or just <CR> to accept: "
    if [ "$ASSUMEDEFAULTUSERINPUT" == 0 ]; then
         read SUBSTRING
    else
         $ECHO "Accepting default."
         SUBSTRING=""
    fi
    $ECHO ""

    if [ ! "$SUBSTRING" == "" ]; then
          RESTOREPATH=`ls -trd $DEFAULTUPDATEPATH/*$SUBNAME*.zip 2>/dev/null | grep update | grep $SUBSTRING | tail -1`
    else
          RESTOREPATH=`ls -trd $DEFAULTUPDATEPATH/*$SUBNAME*.zip 2>/dev/null | grep update | tail -1`
    fi
    if [ "$RESTOREPATH" = "" ]; then
          $ECHO "Error: no matching backups found, aborting"
          exit 2
    fi

    if [ "$RESTOREPATH" == "/sdcard/update.zip" ]; then
        $ECHO "You chose update.zip, it is ready for flashing, there nothing to do."
    else

        # Things seem ok so far.

        # Move the previous update aside, if things go badly with the new update, it is good
        # have the last one still around :-)

        # If we cannot figure out what the file name used to be, create this new one with a time stamp
        OLDNAME="OLD-update-`date +%Y%m%d-%H%M`"

        if [ -e /sdcard/update.zip ]; then
            $ECHO "There is already an update.zip in /sdcard, backing it up to"
            if [ -e /sdcard/update.name ]; then
                OLDNAME=`cat /sdcard/update.name`
                # Backup the name file (presumably contains the old name of the update.zip
                mv -f /sdcard/update.name /sdcard/`basename $OLDNAME .zip`.name
            fi
            $ECHO "`basename $OLDNAME .zip`.zip"
            mv -f /sdcard/update.zip /sdcard/`basename $OLDNAME .zip`.zip

            # Backup the MD5sum file
            if [ -e /sdcard/update.MD5sum ]; then
                mv -f /sdcard/update.MD5sum /sdcard/`basename $OLDNAME .zip`.MD5sum
            fi
        fi

        if [ -e $DEFAULTUPDATEPATH/`basename $RESTOREPATH .zip`.MD5sum ]; then
            mv -f $DEFAULTUPDATEPATH/`basename $RESTOREPATH .zip`.MD5sum /sdcard/update.MD5sum
        else
            $ECHO `md5sum $RESTOREPATH | tee /sdcard/update.MD5sum`
            $ECHO ""
            $ECHO "MD5sum has been stored in /sdcard/update.MD5sum"
            $ECHO ""
        fi
        if [ -e $DEFAULTUPDATEPATH/`basename $RESTOREPATH .zip`.name ]; then
            mv -f $DEFAULTUPDATEPATH/`basename $RESTOREPATH .zip`.name /sdcard/update.name
        else
            echo "`basename $RESTOREPATH`" > /sdcard/update.name
        fi

        mv -i $RESTOREPATH /sdcard/update.zip


        $ECHO "Your file $RESTOREPATH has been moved to the root of sdcard, and is ready for flashing!!!"

    fi

    $ECHO "You may want to execute 'reboot recovery' and then choose the update option to flash the update."
    $ECHO "Or in the alternative, shutdown your phone with reboot -p, and then press <CAMERA>+<POWER> to"
    $ECHO "initiate a standard update procedure if you have stock SPL."
    $ECHO ""
    $ECHO "Cleaning up and exiting."
    umount /sdcard 2>/dev/null
    exit 0
fi
