# Gehe in den Downloads-Ordner
cd /Users/boyan1/Downloads

# Radikaler Löschvorgang (Alles weg!)
rm -rf Lilu MacKernelSDK WhateverGreen

# Frisch klonen (Nur von offiziellen Quellen)
git clone https://github.com/albert-mueller/Lilu/tree/master
git clone https://github.com/acidanthera/MacKernelSDK
git clone https://github.com/albert-mueller/WhateverGreen

# Bauen in der richtigen Reihenfolge
cd Lilu
ln -s ../MacKernelSDK MacKernelSDK
xcodebuild -configuration Debug
cd ../WhateverGreen
ln -s ../MacKernelSDK MacKernelSDK
ln -s ../Lilu/build/Debug/Lilu.kext Lilu.kext
xcodebuild -configuration Debug
