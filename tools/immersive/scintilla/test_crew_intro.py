"""Offline SC5 briefing regressions: no app, compiler or playback."""
from pathlib import Path
import unittest
from unittest.mock import patch
import create_mission_intro as intro


class CrewIntro(unittest.TestCase):
    def test_one_and_two_supporters_have_universal_competence_in_actual_panel(self):
        bold=Path('/System/Library/Fonts/Supplemental/Arial Bold.ttf')
        regular=Path('/System/Library/Fonts/Supplemental/Arial.ttf')
        if not bold.is_file() or not regular.is_file(): self.skipTest('Authoring fonts unavailable')
        original=intro.text
        for party in (False,True):
            for variant,count in [('C1',1),('C2',2)]:
                labels=[]
                def capture(draw,at,value,*args,**kwargs):
                    labels.append(value);return original(draw,at,value,*args,**kwargs)
                with patch.object(intro,'text',capture): intro.crew_panel(variant,1,bold,regular,party)
                self.assertEqual(labels.count('Universale'),count)
                self.assertNotIn('Hacker',labels);self.assertNotIn('Cannoniere',labels)
                self.assertIn('Il kit è universale.',labels)
                self.assertNotIn('Le competenze ruotano.',labels)

    def test_party_briefing_contains_consent_cost_veto_and_release(self):
        bold=Path('/System/Library/Fonts/Supplemental/Arial Bold.ttf')
        regular=Path('/System/Library/Fonts/Supplemental/Arial.ttf')
        if not bold.is_file() or not regular.is_file(): self.skipTest('Authoring fonts unavailable')
        for variant,_,_ in intro.VARIANTS:
            _,party=intro.crew_panel(variant,5,bold,regular,True)
            _,standard=intro.crew_panel(variant,5,bold,regular,False)
            for words in ('Turbo crew','frenare','15','0,75','3 s','zone ammesse'): self.assertIn(words,party)
            self.assertNotEqual(party,standard)
            self.assertNotIn('Turbo crew',standard)

    def test_three_plus_uses_plan_competence_names(self):
        self.assertEqual(intro.crew_roles_for_variant('C3plus'),['Hacker','Cannoniere','Difensore','Ingegnere','Ricognitore'])


if __name__=='__main__':unittest.main()
