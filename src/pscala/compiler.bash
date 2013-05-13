set -e

sources=""
jars=""
while [[ "$1" != "" ]]
do
    if [[ "$1" == "-o" ]]
    then
	outfile="$2"
	shift
	shift
    elif [[ "$1" == "-l" ]]
    then
	jar="$(find /usr/lib $HOME/.local/lib -name "$2".jar)"
	jars="$jar $jars"
	shift
	shift
    else
	sources="$1 $sources"
	shift
    fi
done
workfile="$(basename "$infile")"

if [[ "$(echo $jars)" != "" ]]
then
    classpath="-classpath $jars"
fi

# Make a temporary working directory
#workdir=`mktemp -d -t pscalac.XXXXXX`
#trap "set +e; rm -rf $workdir; rm -f $outfile" EXIT
workdir="$(readlink -f $(pwd))"/"$(dirname "$outfile")"/pscalac-cache
mkdir -p "$workdir"

workjar="$workdir"/out.jar

# Copy the Scala file into the current working directory.  We need to
# make sure it's at the correct path, as otherwise I think it won't
# get loaded correctly.
if which zinc >& /dev/null
then
    strace -f -o "$workdir"/strace -e stat \
	zinc -q -analysis-cache "$workdir"/zinc.cache \
	$sources -d "$workdir" $classpath
else
    strace -f -o "$workdir"/strace -e stat \
	scalac -make:changed $sources -d "$workdir" $classpath
fi

# There isn't a real way to get a list of clas files that Scala
# outputs, so I guess at it using strace... :)
cd "$workdir"
cat "$workdir"/strace | cut -d' ' -f 2- | sed 's/^ //g' \
    | grep "^stat" | sed 's@stat("\(.*\)",.*@\1@' \
    | grep "^$workdir" | (while read f; do test ! -f $f || echo $f; done; ) \
    | sed "s@$workdir/*@@" | grep ".class$" \
    | xargs jar cf "$workjar"
cd - >& /dev/null

# Finally, copy the file
cp "$workjar" "$outfile"

# Clean up
#rm -rf "$workdir"
#trap "true" EXIT