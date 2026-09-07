# Run with the client's Python 2.7 runtime; no client imports or rendering needed.
path = 'C:/Users/skema/Desktop/ClientIgnition/Eternexus/root/uiprivateshop.py'
source = open(path, 'rb').read()
compile(source, path, 'exec')
method = source.split('\tdef __RefreshLifetime(self):', 1)[1].split('\tdef OnUpdate(self):', 1)[0]
exec('class TimerTest:\n\tdef __RefreshLifetime(self):' + method + '\n\tdef refresh(self): self.__RefreshLifetime()\n')
class Stub:
    pass
class Label:
    def SetText(self, value): self.text = value
    def SetFontColor(self, *value): self.color = value
app = Stub()
app.GetGlobalTimeStamp = lambda: 1000
privateShop = Stub()
privateShop.STATE_CLOSED = 1
privateShop.STATE_RECOVERY = 4
privateShop.GetMyState = lambda: 2
privateShop.GetLifetimeSeconds = lambda: 120
localeInfo = Stub()
localeInfo.PREMIUM_PRIVATE_SHOP_TIME_EXPIRED = 'expired'
timer = TimerTest()
timer.remainTimeText = Label()
timer.shopNoticeText = Label()
timer.MODE_BUILD = 1
timer.mode = 2
for remaining, text, color in [(120,'02:00',(0.3,0.9,0.4)), (73,'01:13',(0.3,0.9,0.4)), (72,'01:12',(0.3,0.65,1.0)), (18,'00:18',(0.3,0.65,1.0)), (17,'00:17',(1.0,0.3,0.3)), (0,'00:00',(1.0,0.3,0.3))]:
    privateShop.GetPremiumTime = lambda: 1000 + remaining
    timer.refresh()
    assert timer.remainTimeText.text == text
    assert timer.remainTimeText.color == color
privateShop.GetPremiumTime = lambda: 1000 + 172800
privateShop.GetLifetimeSeconds = lambda: 172800
timer.refresh()
assert timer.remainTimeText.text == '48:00:00'
privateShop.GetMyState = lambda: 4
timer.refresh()
assert timer.remainTimeText.text == '00:00'
assert timer.shopNoticeText.text == 'expired'
timer.mode = 1
timer.refresh()
assert timer.remainTimeText.text == '--:--'
print('PASS: Python 2.7 syntax, seconds, 60/15 percent boundaries, 48h, recovery, build mode')
