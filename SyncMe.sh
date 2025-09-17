# CheckSum="-c"
CheckSum=""
Drive="ext4tb01"

echo "Syncing: CloudDocs"
rsync -avL ${CheckSum} --delete --exclude-from=/Users/${USER}/.rsync-ignore "/Users/${USER}/Library/Mobile Documents/com~apple~CloudDocs" /Volumes/${Drive}

echo "Syncing: PhotoArch"
rsync -av ${CheckSum} --delete --exclude-from=/Users/${USER}/.rsync-ignore /Volumes/dockdata/PhotoArch /Volumes/${Drive}

echo "Syncing: Pictures"
rsync -av ${CheckSum} --delete --exclude-from=/Users/${USER}/.rsync-ignore /Users/${USER}/Pictures/Photos Library.photoslibrary /Volumes/${Drive}

echo "Syncing: VideArch"
rsync -av ${CheckSum} --delete --exclude-from=/Users/${USER}/.rsync-ignore /Volumes/dockdata/VideoArch /Volumes/${Drive}

