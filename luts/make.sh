
mkdir -p output

for FILE in *.cube; do 
  echo "processing $FILE..."
  magick "cube:$FILE[8]" hald.png
  convert neutral-lut.png hald.png -hald-clut "output/$(basename "$FILE" .cube).png"

done

