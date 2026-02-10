# Open Partition Manager Documentation

This is the Docusaurus-powered documentation website for Open Partition Manager (OPM).

## Quick Start

### Development

```bash
npm install
npm start
```

Visit `http://localhost:3000`

### Build

```bash
npm run build
```

Static files are generated in `build/` directory.

## Deployment

### Coolify Deployment

This project is configured for easy deployment to Coolify.

#### Option 1: Using Nixpacks (Recommended)

Coolify will automatically detect `nixpacks.toml` and use it for deployment.

1. Connect your GitHub repository to Coolify
2. Select "Nixpacks" as the build method
3. Deploy

The site will be served at the configured domain.

#### Option 2: Using Dockerfile

If you prefer Docker:

1. Connect your GitHub repository to Coolify
2. Select "Dockerfile" as the build method
3. Deploy

The Dockerfile is optimized for production with multi-stage builds.

### Manual Deployment

#### Static Hosting (Netlify, Vercel, etc.)

```bash
npm run build
# Upload build/ directory to your host
```

#### Docker

```bash
# Build image
docker build -t opm-docs .

# Run container
docker run -p 80:80 opm-docs
```

## Project Structure

```
├── blog/              # Blog posts
├── docs/              # Documentation
│   ├── intro.md       # Introduction
│   ├── getting-started/
│   ├── features/
│   ├── development/
│   ├── roadmap.md
│   └── faq.md
├── src/               # Source files
│   ├── css/           # Custom styles
│   └── pages/         # React pages
├── static/            # Static assets
│   └── img/           # Images
├── docusaurus.config.ts  # Configuration
├── sidebars.ts        # Sidebar config
└── package.json       # Dependencies
```

## Documentation

- **Introduction**: Overview of OPM
- **Getting Started**: Installation and setup
- **Features**: Feature documentation
- **Development**: Architecture and contributing
- **Roadmap**: Development timeline
- **FAQ**: Common questions

## Contributing

See the main OPM repository for contribution guidelines.

## License

GPL-3.0
