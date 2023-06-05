import pprint
import sqlite3

DB = 'discogs.db'

def connect():
  return sqlite3.connect(DB)

def _aliases(artist_id):
  result = [str(artist_id)]
  result.extend([str(x[0]) for x in con.execute(f'''select t from artist_alias where f={artist_id}''')])
  return ', '.join(result)

def labels_of_artist(artist_id):
  result = []
  result.extend(con.execute(f'''
select id,name from labels where id in (
  select t from release_label where f in (
    select f from release_artist where t in (
      {_aliases(artist_id)}
    )
  )
)
  '''))
  return [(x[0], x[1], 'https://www.discogs.com/label/{}'.format(x[0])) for x in result]

def releases_of_artist(artist_id):
  result = []
  result.extend(con.execute(f'''

select id,name from releases where id in (
  select f from release_artist where t in (
    {_aliases(artist_id)}
  )
)
  '''))
  return [(x[0], x[1], 'https://www.discogs.com/release/{}'.format(x[0])) for x in result]

def artists_on_label(label_id):
  result = []
  result.extend(con.execute(f'''
select id,name from artists where id in (
  select t from release_artist where f in (
    select f from release_label where t={label_id}
    )
  )
  '''))
  return [(x[0], x[1], 'https://www.discogs.com/artist/{}'.format(x[0])) for x in result]

con = connect()

def walk(depth, max_depth, seen_artists, seen_labels, mode_artist, root_record):
  if depth == max_depth:
    return

  root_id = root_record[0]

  if not mode_artist and root_id in [1818, 1866, 2311, 8377]:
    return
  
  if mode_artist and root_id in seen_artists:
    return
  if not mode_artist and root_id in seen_labels:
    return

  print(' ' * depth + str(root_record))
  
  if mode_artist:
    new_seen_artists = seen_artists.union([root_id])
    new_seen_labels = seen_labels
  else:
    new_seen_artists = seen_artists
    new_seen_labels = seen_labels.union([root_id])

  if mode_artist:
    frontier = labels_of_artist(root_id)[0:10]
  else:
    frontier = artists_on_label(root_id)[0:10]

  for record in frontier:
    walk(depth + 1, max_depth, new_seen_artists, new_seen_labels, not mode_artist, record)

def walk_start(max_depth, mode_artist, root):
  walk(0, max_depth, set(), set(), mode_artist, root)

try:
  walk_start(3, False, (1862921, 'Small Moves', 'https://www.discogs.com/label/1862921'))
except BrokenPipeError:
  pass

con.close()
