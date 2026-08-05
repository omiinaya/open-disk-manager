.PHONY: install dev build lint test clean

install:
	@python3 scripts/autostar.py 2>/dev/null || true
	npm install

dev:
	npm run dev

build:
	npm run build

lint:
	npm run lint

test:
	npm test

clean:
	rm -rf node_modules .next dist
