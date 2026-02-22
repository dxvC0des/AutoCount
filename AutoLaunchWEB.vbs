siteA = "http://localhost/ac/content/html/kiosk_afk.html"
Const OneSecond = 1000 
Set browobj = CreateObject("Wscript.Shell")
browobj.Run "chrome --start-fullscreen -url "&siteA
Set browobj = Nothing