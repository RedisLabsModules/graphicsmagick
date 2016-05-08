# rm_graphicsmagick: Redis Module wrapping libGraphicsMagick

Provides (some of) the functionality of the [GraphicsMagick](http://www.graphicsmagick.org) image processing system inside of redis.
Currently very limited functionality is supported more as a POC than anything practical, but it can fairly easily be extended to do whatever "magick" you want.

## Building it

### Prerequisites

* libGraphicsMagic.so and header should be installed (ubuntu: apt-get install libgraphicsmagick1-dev), tested with version 1.3.23

### Bulding the module

* in the source folder run `make`

## Running it

* Run `redis-server --loadmodule <path-to-module>/rm_graphicsmagick.so`

## Image manipulation API

All commands access a string key containing an image in a format GraphicsMagick can handle (See: http://www.graphicsmagick.org/formats.html).

### `GRAPHICSMAGICK.ROTATE key angle`
Rotate image stored in `key` and updates `key` with the result. `angle` is a float value in degrees for rotation.

**Reply:** String, "OK" on success. Error on error.

### `GRAPHICSMAGICK.SWIRL key angle`

Swirl image stored in `key` and updates `key` with the result. `angle` is a float value in degrees for swirling.

**Reply:** String, "OK" on success. Error on error.

### `GRAPHICSMAGICK.BLUR key radius std_dev`

Blur image stored in `key` and update `key` with the result. `radius` is the Gaussian radius in pixels, `std_dev` is the standard deviation of the Gaussian.

**Reply:** String, "OK" on success. Error on error.

### `GRAPHICSMAGICK.THUMBNAIL key width height`

Scale image stored in `key` to given `width` and `height` and updates `key` with the result.

**Reply:** String, "OK" on success. Error on error.

### `GRAPHICSMAGICK.TYPE key`

 Return the format of the image stored in `key` ("PNG", "GIF", etc.). Returns an error if no valid image is stored in `key`.

**Reply:** String or error.

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

## License

see [LICENSE](LICENSE)
