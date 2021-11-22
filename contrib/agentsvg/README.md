# CFEngine agent SVG / logo generator

![cf-agent, the CFEngine mascot](agent.png)

Python script(s) for generating agent logos.

## The default logo

```
python3 agentsvg.py > agent.svg
```

## Logo with custom colors

```
python3 agentsvg.py --body="#f5821f" --head="#052569" > custom-agent.svg
```

## Converting to PNG

```
convert -background none agent.svg agent.png
```

(Requires ImageMagick `convert`).

## Generating PNGs and SVGs for all resolutions, color combinations, and poses

```
python3 generate_all.py
```
