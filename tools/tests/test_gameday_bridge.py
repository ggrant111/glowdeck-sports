import json
from pathlib import Path
import tempfile
import unittest
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from gameday_bridge import Controller, BridgeError, compact_nhl, restore_payload

ORIGINAL = {'on': True, 'bri': 191, 'transition': 7,
            'seg': [{'id': 0, 'on': True, 'bri': 174, 'fx': 45, 'sx': 80,
                     'ix': 95, 'pal': 6, 'col': [[100, 50, 20], [0, 0, 0]], 'frz': False},
                    {'id': 1, 'fx': 9, 'col': [[1, 2, 3]]}]}

class FakeWled:
    def __init__(self):
        self.state = json.loads(json.dumps(ORIGINAL))
        self.actions = []
        self.fail_restore = False
    def __call__(self, url, payload=None, timeout=5):
        if url.endswith('/json/state'):
            if payload is not None:
                if self.fail_restore and payload.get('bri') == 191:
                    raise OSError('device disconnected')
                self.actions.append(json.loads(json.dumps(payload)))
                self.state['on'] = payload.get('on',self.state['on'])
                self.state['bri'] = payload.get('bri',self.state['bri'])
                for s in payload.get('seg',[]):
                    for existing in self.state['seg']:
                        if existing['id'] == s['id']: existing.update(s)
            return json.loads(json.dumps(self.state))
        raise AssertionError('Unexpected URL '+url)

class TestBridge(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.device = FakeWled()
        self.c = Controller('192.168.1.71', Path(self.tmp.name)/'recovery.json', fetch=self.device)
    def tearDown(self):
        self.c.restore() if self.c.journal.exists() and not self.device.fail_restore else None
        self.tmp.cleanup()
    def test_nhl_normalization(self):
        data=compact_nhl({'currentDate':'2026-11-01', 'games':[{'id':123,'gameState':'LIVE', 'awayTeam':{'abbrev':'UTA','score':3},'homeTeam':{'abbrev':'BOS','score':1},'period':3,'clock':{'timeRemaining':'04:59'}}]})
        self.assertEqual(data['games'][0]['away'], 'UTA')
        self.assertEqual(data['games'][0]['awayScore'], 3)
        self.assertEqual(data['games'][0]['state'],'LIVE')
        self.assertFalse(data['demo'])
    def test_no_games(self):
        self.assertEqual(compact_nhl({'games':[]})['games'],[])
    def test_restoration_segment_only(self):
        payload=restore_payload(ORIGINAL)
        self.assertEqual(payload['bri'],191)
        self.assertEqual(payload['seg'][0]['fx'],45)
        self.assertEqual(len(payload['seg']),1)
    def test_no_segment_previews_blocked(self):
        with self.assertRaises(BridgeError): restore_payload({'on':True,'seg':[{'id':5}]})
    def test_real_preview_lifecycle(self):
        result=self.c.preview('UTA', 5)
        self.assertTrue(self.c.journal.exists())
        self.assertEqual(result['color'],'#6CACE4')
        self.assertEqual(self.device.state['seg'][0]['col'][0],[108,172,228])
        self.assertEqual(self.device.state['seg'][1]['fx'],9)
        self.c.restore()
        self.assertFalse(self.c.journal.exists())
        self.assertEqual(self.device.state['seg'][0]['fx'],45)
        self.assertEqual(self.device.state['seg'][0]['col'],ORIGINAL['seg'][0]['col'])
        self.assertEqual(self.device.state['bri'],191)
    def test_reboot_recovery(self):
        self.c.preview('BOS', 5)
        self.c.timer.cancel()
        second=Controller('192.168.1.71',self.c.journal,fetch=self.device)
        second.recover_at_startup()
        self.assertFalse(second.journal.exists())
        self.assertEqual(self.device.state['seg'][0]['fx'],45)
    def test_restore_failure_preserves_journal(self):
        self.c.preview('UTA',5)
        self.device.fail_restore=True
        with self.assertRaises(OSError): self.c.restore()
        self.assertTrue(self.c.journal.exists())
        self.device.fail_restore=False
        self.c.restore()
        self.assertFalse(self.c.journal.exists())
    def test_validate_inputs(self):
        for args in [('XYZ', 5), ('UTA', 15), ('UTA', 1), ('UTA', True)]:
            with self.assertRaises(BridgeError): self.c.preview(*args)
        with self.assertRaises(ValueError): Controller('8.8.8.8',Path(self.tmp.name)/'no.json')
    def test_does_not_automatically_contact_device_when_normalizing(self):
        compact_nhl({'games':[]})
        self.assertEqual(self.device.actions, [])

if __name__=='__main__': unittest.main()
