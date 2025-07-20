# macOS DMG 背景设置脚本
tell application "Finder"
    tell disk "CorePlatform"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {400, 100, 900, 400}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 72
        set background picture of viewOptions to file ".background:background.png"
        set position of item "CorePlatform.pkg" of container window to {100, 100}
        set position of item "Applications" of container window to {350, 100}
        close
    end tell
end tell