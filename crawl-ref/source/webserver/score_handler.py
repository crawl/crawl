import tornado.web
import itertools
import locale
import os.path
import re

from conf import config
import mapping

class ScoreTopNHandler(tornado.web.RequestHandler):
    def get(self, score_list_length):
        # score_list_length is in [min_score_list_length, max_score_list_length]
        max_length = config.get("max_score_list_length")
        if not max_length:
            max_length = 1000
        min_length = config.get("min_score_list_length")
        if not min_length:
            min_length = 100

        score_list_length = int(score_list_length)
        score_list_length = min(max_length, score_list_length)
        score_list_length = max(min_length, score_list_length)

        class Score(dict):
            def __getattr__(self, name):
                return self.get(name, '')

        def parse_scores(scores_path):
            def parse_scoreline(line):
                score = Score(line=line) # for debug
                for token in line.strip().split(':'):
                    if token.count('=') == 1:
                        attr, value = token.split('=')
                        score[attr] = value

                # convert to comma-separeted digits
                score['sc'] = locale.format("%d", int(score['sc']), grouping=True)
                score['turn'] = locale.format("%d", int(score['turn']), grouping=True)

                if 'end' in score:
                    # fix 0-origin month to 1-origin
                    YYYYMMDDHHMMSS = str(int(score['end'].rstrip('SD')) + 100000000)
                    ptn = re.compile(r'(\d{2})(\d{2})(\d{2})(\d{2})(\d{6})')

                    score['dumpURL'] = ptn.sub(r'../../morgue/{name}/morgue-{name}-\1\2\3\4-\5.txt'.format(**score), YYYYMMDDHHMMSS)
                    score['date'] = ptn.sub(r'\2/\3/\4', YYYYMMDDHHMMSS)

                return score

            try:
                if not os.path.basename(scores_path) in mapping.scores.keys():
                    raise tornado.web.HTTPError(403)

                with open(scores_path) as f:
                    return [parse_scoreline(line) for line in itertools.islice(f, score_list_length)]
            except IOError as err:
                # If scores is not found:
                return []

        params = {
            'filename': self.get_argument('filename', default='scores'),
            'score_list_length': score_list_length,
            'version': self.get_argument('version', default='trunk'),
        }
        params['version_path'] = mapping.version_path.get(params['version'], 'crawl-ref')

        path = "../../{version_path}/source/saves/{filename}".format(**params)
        params['scores'] = parse_scores(path)

        self.render('top-N.html', **params)
