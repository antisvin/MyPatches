Building resources
==================

1. Install dependencies from requirement.txt, i.e.

::
    mkvirtualenv KastleDrum
    pip install -r requirements.txt

2. Run KastlDrum.py

::
    ./KastleDrum.py

This script would generate binary file with OWL resource - KastleDrum.wav

3. Convert WAV file to OWL resource

::
    cd ../../OwlProgram
    ./Tools/makeresource.py -w -i ../MyPatches/KastleDrum/KastleDrum.wav -o ../MyPatches/KastleDrum/KastleDrum.bin KastleDrum

4. Save file to your device from OwlProgram repo

::
    make RESOURCE=../MyPatches/KastleDrum/KastleDrum.wav SLOT=43 resource 
