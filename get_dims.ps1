Add-Type -AssemblyName System.Drawing
$files = Get-ChildItem "C:\Users\jeffm\Documents\Vibesynth\Files For Claude\Filmstrips\*.png"
foreach($f in $files){
    $img = [System.Drawing.Image]::FromFile($f.FullName)
    "$($f.Name): $($img.Width) x $($img.Height)"
    $img.Dispose()
}
