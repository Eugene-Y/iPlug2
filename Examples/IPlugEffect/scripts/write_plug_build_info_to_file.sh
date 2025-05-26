commit=$(git rev-parse --verify HEAD | cut -c 1-7)
branch_name=$(git rev-parse --abbrev-ref HEAD)

datestr=$(date '+%Y.%m.%d %H:%M:%S')
curdate=$(date +"%d.%m.%y")
curtime=$(date +"%T")

# ARCHS expected to be the last argument 
# some Xcode ARCHS build setting uses \n as separator.
# "$*" puts all arguments into string with spaces.

archs=""
s=""
n=1
for i in $(echo "$*") 
do 
if ((n > 2)); then
archs=$archs$s$i
s=" "
fi
n=$n+1
done

filesource="//
//  plug_build_info.hpp
//
//  Created automatically by Xcode on $curdate at $curtime.
//

#ifndef plug_build_info_hpp
#define plug_build_info_hpp

#define PLUG_GIT_BRANCH_NAME \"$branch_name\"
#define PLUG_GIT_COMMIT_SHA  \"$commit\"
#define PLUG_BUILD_DATE      \"$datestr\"
#define PLUG_PRODUCT         \"$2\"
#define PLUG_ARCHS           \"$archs\"

#endif"

FILE_PATH=$1"plug_build_info.hpp"
echo "$filesource" > "$FILE_PATH"

# разрешения:
# Xcode:  открыть терминал в папке со скриптом, выполнить chmod 755 write_plug_build_info_to_file.sh
# Android Studio: открыть терминал в папке с gradlew, выполнить chmod +x gradlew
