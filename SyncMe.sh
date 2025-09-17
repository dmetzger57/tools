# CheckSum="-c"
CheckSum=""

echo "Syncing: PhotoArch"
rsync -av ${CheckSum} --delete --exclude-from=/Users/${USER}/.rsync-ignore /Volumes/dockdata/PhotoArch /Volumes/MediaArch

echo "Syncing: VideArch"
rsync -av ${CheckSum} --delete --exclude-from=/Users/${USER}/.rsync-ignore /Volumes/dockdata/VideoArch /Volumes/MediaArch

echo "Syncing: CloudDocs"
rsync -avL ${CheckSum} --delete --exclude-from=/Users/${USER}/.rsync-ignore "/Users/${USER}/Library/Mobile Documents/com~apple~CloudDocs" /Volumes/MediaArch
