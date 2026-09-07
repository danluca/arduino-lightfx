## Copyright (c) 2026 Dan Luca. All rights reserved.
#######################################
## Global
#######################################
class Board {
    [int]$Id
    [string]$Name
    [string]$IpAddress

    Board([int]$id, [string]$name, [string]$ipAddress) {
        $this.Id = $id
        $this.Name = $name
        $this.IpAddress = $ipAddress
    }
}

# Board name → URI mapping
$boardMap = @{
    'Dev'  = [Board]::new(1, 'Dev', 'http://192.168.0.75')
    'Tree' = [Board]::new(2, 'Tree', 'http://192.168.0.182')
}

#######################################
## Functions
#######################################

function Get-BoardByName([string]$name) {
    if ($boardMap.ContainsKey($name)) {
        return $boardMap[$name]
    } else {
        throw "Board '$name' not found in board map."
    }
}