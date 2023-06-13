#######################################
## Functions
#######################################
function makeHeaderFile([System.IO.FileInfo]$file) {
    pushd ..

    $fname = $file.Name -replace '-|\.','_'
    $constStrStart = "const char $fname[] PROGMEM = R`"===("
    $constStrEnd = ")===`";"
    $f=New-Item -Path "$($file.Name -replace '-|\.','_').h" -ItemType File -Force
    $constStrStart | Out-File $f -Append
    Get-Content $file.FullName -Raw | Out-File $f -Append
    $constStrEnd | Out-File $f -Append 

    popd
}

#######################################
## Main
#######################################
pushd $PSScriptRoot
pushd ../www

Get-ChildItem -Attributes !D | ForEach-Object {makeHeaderFile $_}

popd
popd
