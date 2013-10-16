echo Compiling... 
cd app
make clean
make T=v300 all
mv softupdate ../softupdate
echo compile is OK.

