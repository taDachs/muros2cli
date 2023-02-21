# muros2cli

An attempt at bringing **ROS1** cli tools like `rosed` and `rospack` to **ROS2**. So far it provides
one executable that tries to imitate some functionality of `rospack`.

## Install

Simply run the Makefile and link the executable somewhere in your path:

```sh
make
ln -s $PWD/muros2 ~/.local/bin  # for example
```

## Usage

### `path`

Outputs the path to your last sourced workspace

```sh
muros2 path
```

### `list-paths`

Outputs the paths to all your sourced workspaces

```sh
muros2 list-paths
```

### `list`

List all sourced packages with path to source.

```sh
muros2 list
```

### `find`

Outputs the path to the source location of the given package.

```sh
muros2 find [pkg name]
```


### Missing features

- all arguments/flags
- `rosmsg`/`rossrv` (I dont like `ros2 interface`)
- `rosed`
- `roscd`
