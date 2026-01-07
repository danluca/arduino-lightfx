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
    'Dev'  = [Board]::new(1, 'Dev', 'http://192.168.0.10')
    'FX01' = [Board]::new(2, 'FX01', 'http://192.168.0.11')
    'FX02' = [Board]::new(3, 'FX02', 'http://192.168.0.12')
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