#!/bin/sh
set -e

# In das Verzeichnis des Skripts wechseln
cd "$(dirname "$0")"

# 1. Alles sauber zurücksetzen (verhindert alte Inkompatibilitäten)
echo "Bereinige alte Build-Daten..."
rm -rf Lilu MacKernelSDK WhateverGreen

# 2. Repositories klonen (Offizielle Acidanthera-Quellen)
echo "Klone Repositories..."
git clone https://github.com/acidanthera/Lilu
git clone https://github.com/acidanthera/MacKernelSDK
git clone https://github.com/acidanthera/WhateverGreen

# 3. Lilu bauen
echo "Baue Lilu..."
cd Lilu
ln -s ../MacKernelSDK MacKernelSDK
xcodebuild -configuration Debug
cd ..

# 4. WhateverGreen bauen
echo "Baue WhateverGreen..."
cd WhateverGreen
ln -s ../MacKernelSDK MacKernelSDK
ln -s ../Lilu/build/Debug/Lilu.kext Lilu.kext
xcodebuild -configuration Debug
cd ..

echo "------------------------------------------------"
echo "Erfolgreich gebaut!"
echo "Lilu: Lilu/build/Debug/Lilu.kext"
echo "WhateverGreen: WhateverGreen/build/Debug/WhateverGreen.kext"
