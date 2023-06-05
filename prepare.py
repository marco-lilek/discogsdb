import sqlite3

DB = 'discogs.db'
ARTISTS_TOK =  'tokenizer/artists.tok'
LABELS_TOK =   'tokenizer/labels.tok'
RELEASES_TOK = 'tokenizer/releases.tok'

con = sqlite3.connect(DB)
con.execute("PRAGMA synchronous = OFF")
con.execute("PRAGMA journal_mode = OFF")

con.execute(''' CREATE TABLE artists (id INTEGER PRIMARY KEY, name BLOB) ''')

con.execute(''' CREATE TABLE labels (id INTEGER PRIMARY KEY, name BLOB) ''')
con.execute(''' CREATE TABLE releases (id INTEGER PRIMARY KEY, name BLOB) ''')

con.execute(''' CREATE TABLE label_sublabel (f INT, t INT) ''')

con.execute(''' CREATE TABLE artist_member (f INT, t INT) ''')

con.execute(''' CREATE TABLE artist_alias (f INT, t INT) ''')

con.execute(''' CREATE TABLE release_artist (f INT, t INT) ''')

con.execute(''' CREATE TABLE release_label (f INT, t INT) ''')

ROOT = 'INSERT INTO {} VALUES (?, ?)'
ARTISTS = ROOT.format('artists')
LABELS = ROOT.format('labels')
RELEASES = ROOT.format('releases')

LABEL_SUBLABEL = ROOT.format('label_sublabel')
ARTIST_MEMBER = ROOT.format('artist_member')
ARTIST_ALIAS = ROOT.format('artist_alias')
RELEASE_ARTIST = ROOT.format('release_artist')
RELEASE_LABEL = ROOT.format('release_label')


with open(ARTISTS_TOK, 'r') as f:
  i = None
  name = None
  finished = 0

  for line in f:
    k = line[0] 
    if k == 'I': i = line[1:]
    elif k == 'N': name = line[1:].rstrip()
    elif k == 'A':
      alias = line[1:]
      con.execute(ARTIST_ALIAS, (i, alias))
    elif k == 'M':
      member = line[1:]
      con.execute(ARTIST_MEMBER, (i, member))
    elif k == 'D': # reset
      con.execute(ARTISTS, (i, name))
      i = None
      name = None
      finished += 1
      if finished % 100000 == 0:
        print('done artists {}'.format(finished))

with open(LABELS_TOK, 'r') as f:
  i = None
  name = None
  finished = 0

  for line in f:
    k = line[0] 
    if k == 'I': i = line[1:]
    elif k == 'N': name = line[1:].rstrip()
    elif k == 'S':
      s = line[1:]
      con.execute(LABEL_SUBLABEL, (i, s))
    elif k == 'D': # reset
      con.execute(LABELS, (i, name))
      i = None
      name = None
      finished += 1
      if finished % 100000 == 0:
        print('done labels {}'.format(finished))

with open(RELEASES_TOK, 'r') as f:
  i = None
  name = None
  finished = 0

  for line in f:
    k = line[0] 
    if k == 'I': i = line[1:]
    elif k == 'N': name = line[1:].rstrip()
    elif k == 'R' or k == 'E' or k == 'T':
      v = line[1:]
      con.execute(RELEASE_ARTIST, (i, v))
    elif k == 'L':
      v = line[1:]
      con.execute(RELEASE_LABEL, (i, v))
    elif k == 'D': # reset
      con.execute(RELEASES, (i, name))
      i = None
      name = None
      finished += 1
      if finished % 100000 == 0:
        print('done releases {}'.format(finished))

print('creating indexes')
con.execute(''' CREATE INDEX idx_label_sublabel on label_sublabel (f) ''')
con.execute(''' CREATE INDEX idx_sublabel_label on label_sublabel (t) ''')

con.execute(''' CREATE INDEX idx_artist_member on artist_member (f) ''')
con.execute(''' CREATE INDEX idx_member_artist on artist_member (t) ''')

con.execute(''' CREATE INDEX idx_artist_alias on artist_alias (f) ''')
con.execute(''' CREATE INDEX idx_alias_artist on artist_alias (t) ''')

con.execute(''' CREATE INDEX idx_release_artist on release_artist (f) ''')
con.execute(''' CREATE INDEX idx_artist_release on release_artist (t) ''')

con.execute(''' CREATE INDEX idx_release_label on release_label (f) ''')
con.execute(''' CREATE INDEX idx_label_release on release_label (t) ''')

con.commit()
con.close()
