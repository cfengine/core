# Cowsay for cf-agents

## Example output
```
/-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\
|                               |
|          HELLO, AGENT         |
|                               |
|             ● ● ●             |
|             ● ● ●             |
|                               |
|         ●   ● ● ●   ●         |
|         ●   ● ● ●   ●         |
|         ●   ● ● ●   ●         |
|         ●           ●         |
|             ● ● ●             |
|             ●   ●             |
|             ●   ●             |
|             ●   ●             |
|                               |
\-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-/
```

## Download
```
git clone https://github.com/cfengine/core.git
cd core/contrib/agentsay
```

## Python Install
```
python3 setup.py
```

## CLI Install
```
cp agentsay.py /usr/local/bin/agentsay && chmod ugo+x /usr/local/bin/agentsay && agentsay install success
```

## CLI usage
```
agentsay Hello, agent
```

## Python usage
```Python
import agentsay
agentsay.speak("Hello, agent")
```

## Copyright
Copyright Northern.tech AS
