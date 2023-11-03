#######################################
## Functions
#######################################
function makeHeaderFile([System.IO.FileInfo]$file) {
    pushd ../../include

    $fname = $file.Name -replace '-|\.','_'
    $constStrStart = "const char $fname[] PROGMEM = R`"~~~("
    $constStrEnd = ")~~~`";"
    $f=New-Item -Path "$($fname).h" -ItemType File -Force
    $constStrStart | Out-File $f -Append -Encoding utf8
    Get-Content $file.FullName -Raw -Encoding utf8 | Out-File $f -Append -Encoding utf8
    $constStrEnd | Out-File $f -Append -Encoding utf8

    popd
}

#######################################
## Main
#######################################
pushd $PSScriptRoot
pushd ../src/www

Get-ChildItem * -Attributes !D -Exclude *.json | ForEach-Object {makeHeaderFile $_}

popd
popd
