DRIVE=MediaArch-Clone-4

# CheckSum="-c"
CheckSum=""

SYSTEM=`uname -n | cut -d'.' -f1`
if [ "${SYSTEM}" == "iMac" ]
then
	RSYNC="/usr/local/bin/rsync"
else
	RSYNC="rsync"
fi

echo "Syncing: CloudDocs"
${RSYNC} -avL ${CheckSum} --delete --exclude-from=/Users/${USER}/.rsync-ignore --info=progress3,stats "/Users/${USER}/Library/Mobile Documents/com~apple~CloudDocs" /Volumes/${DRIVE}

echo "Syncing: PhotoArch"
${RSYNC} -av ${CheckSum} --delete --exclude-from=/Users/${USER}/.rsync-ignore --info=progress3,stats /Volumes/dockdata/PhotoArch /Volumes/${DRIVE}

echo "Syncing: VideArch"
${RSYNC} -av ${CheckSum} --delete --exclude-from=/Users/${USER}/.rsync-ignore --info=progress3,stats /Volumes/dockdata/VideoArch /Volumes/${DRIVE}
