#######################################
## Functions
#######################################
function makeHeaderFile([System.IO.FileInfo]$file) {
    pushd ../../include

    $fname = $file.Name -replace '-|\.','_'
    $constStrStart = "const char $fname[] PROGMEM = R`"===("
    $constStrEnd = ")===`";"
    $f=New-Item -Path "$($fname).h" -ItemType File -Force
    $constStrStart | Out-File $f -Append
    Get-Content $file.FullName -Raw | Out-File $f -Append
    $constStrEnd | Out-File $f -Append 

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
