# Redis Module wrapping libGraphicsMagick

Currently very limited functionality is supported more as a POC than anything practical.

## Building it

### Prerequisites

* libGraphicsMagic.so and header should be installed (ubuntu: apt-get install libgraphicsmagick1-dev), tested with version 1.3.23

### Bulding the module

* in the source folder run `make`

## Running it

* Run `redis-server --loadmodule <path-to-module>/rm_graphicsmagick.so`

## Using it

All commands manipulate a string key containing an image in a format GraphicsMagick can handle (See: http://www.graphicsmagick.org/formats.html).

### Added commands

* GRAPHICSMAGICK.ROTATE <KEY> <float angle in degrees> - rotate image
* GRAPHICSMAGICK.SWIRL <KEY> <float angle in degrees> - swirl image
* GRAPHICSMAGICK.BLUR <KEY> <float Gaussian radius in pixels> <float std deviation of the Gaussian> - blur image
* GRAPHICSMAGICK.THUMBNAIL <KEY> <int width of scaled image> <int height of scaled image> - scale image
* GRAPHICSMAGICK.TYPE <KEY> - return image format

### Python example
```python
import redis
r = redis.Redis()
with open('/tmp/some_image.png') as f:
    r.set('image_key', f.read())
r.execute_command('GRAPHICSMAGICK.SWIRL', 'image_key', 232.3)
with open('/tmp/swirled_image.png', 'w') as f:
    f.write(r.get('image_key'))
```

## Extending it

See here for details of GraphicsMagick core API: http://www.graphicsmagick.org/api/api.html
Look at the implementation of the actually commads `GMXXXCommand()` functions and add new ones accordingly.

There are a million good ideas about what more can be done with this module, here are some:
* STORE stye commands so key isn't modified.
* Format conversion commands.
* Multi key commands for composing multiple images into new images.
* Profiling performance and finding optimal formats for doing the manipulation including working with RAW formats and perhaps doing the transformantion in place without copying the data.
* ...
