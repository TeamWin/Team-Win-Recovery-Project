#!/bin/sh
#
#
[ $# -ne 1 ] && echo "$0 deviceid [backup|restore]" && exit 2 

task=$1
devid=`cat /proc/cmdline | sed \"s/.*serialno=//\" | cut -d\" \" -f1`
sapath="/system/app/"
sepath="/system/etc/"
slpath="/system/lib/"
sfpath="/system/framework/"
bkpath="/sdcard/nandroid/gapps/${devid}/"

if [ "$task" == "backup" ]; then

	for file in ${sapath}GoogleServicesFramework.apk ${sapath}GoogleFeedback.apk ${sapath}GoogleCalendarSyncAdapter.apk ${sapath}GoogleContactsSyncAdapter.apk ${sapath}GoogleBackupTransport.apk ${sapath}CarHomeGoogle.apk ${sapath}SetupWizard.apk ${sapath}BooksPhone.apk ${sapath}Vending.apk ${sapath}FOTAKill.apk ${sapath}MediaUploader.apk ${sapath}NetworkLocation.apk ${sapath}Talk.apk ${sapath}GoogleQuickSearchBox.apk ${sapath}LatinImeTutorial.apk ${sapath}MarketUpdater.apk ${sapath}OneTimeInitializer.apk ${sapath}GenieWidget.apk ${sapath}GooglePartnerSetup.apk ${sepath}permissions/com.google.android.maps.xml ${sepath}permissions/features.xml ${slpath}libvoicesearch.so ${sfpath}com.google.android.maps.jar
	do
		if [ -f $file ]; then
			echo "$file"
		fi
	
	done | xargs tar cf ${bkpath}bakgapps.tar

elif [ "$task" == "restore" ]; then

	cd /
	tar xf ${bkpath}bakgapps.tar

else
	echo "command line arg $task unknown, options are [backup|restore]"
	exit 1
fi
