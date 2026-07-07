#!/bin/sh
set -e

cd /Users/boyan1/Downloads

# 1. Radikal aufräumen
rm -rf Lilu MacKernelSDK WhateverGreen

# 2. OFFIZIELLE Quellen klonen (Hier liegt die Lösung für deinen Build-Fehler)
git clone https://github.com/acidanthera/Lilu
git clone https://github.com/acidanthera/MacKernelSDK
git clone https://github.com/acidanthera/WhateverGreen

# 3. Lilu bauen
cd Lilu
ln -s ../MacKernelSDK MacKernelSDK
# Wir nutzen -project für maximale Stabilität
xcodebuild -project Lilu.xcodeproj -configuration Debug
cd ..

# 4. WhateverGreen bauen
cd WhateverGreen
ln -s ../MacKernelSDK MacKernelSDK
# Lilu als Symlink für WhateverGreen verknüpfen
ln -s ../Lilu/build/Debug/Lilu.kext Lilu.kext
# Bauen
xcodebuild -project WhateverGreen.xcodeproj -configuration Debug
cd ..

echo "Build erfolgreich abgeschlossen!"
