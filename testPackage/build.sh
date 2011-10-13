#!/bin/sh

cd res
zip -r ../cm7-evo3d/META-INF/teamwin/twrp.zip *
cd ..

cd cm7-evo3d
zip -r ../cm7-evo3d-unsigned.zip *
cd ..
java -classpath testsign.jar testsign cm7-evo3d-unsigned.zip cm7-evo3d.zip
rm cm7-evo3d-unsigned.zip

