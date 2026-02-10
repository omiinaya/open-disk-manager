# Deployment Guide

## Coolify Deployment

### Prerequisites

- A Coolify instance
- Git repository with your code

### Step-by-Step

1. **Push to Git**
   ```bash
   git add .
   git commit -m "Update Docusaurus documentation"
   git push origin main
   ```

2. **Add Resource in Coolify**
   - Go to your Coolify dashboard
   - Click "Add Resource"
   - Select "Application"

3. **Configure Source**
   - Select "GitHub" or "GitLab"
   - Choose your repository
   - Select branch (e.g., `main`)

4. **Build Method**
   
   **Select "Dockerfile"**
   - Coolify will automatically detect the `Dockerfile` in the root
   - This repository has both C++ code and Docusaurus docs
   - The Dockerfile is configured to build only the documentation site
   - No additional configuration needed

5. **Deploy**
   - Click "Deploy"
   - Wait for build to complete
   - Your site is live!

### Environment Variables

No environment variables required for basic deployment.

### Custom Domain

1. In Coolify, go to "Domains"
2. Add your custom domain
3. Configure DNS to point to Coolify
4. SSL certificates are automatically managed

## Troubleshooting

### "Nixpacks failed to detect the application type"

This error occurs because the repository has both `CMakeLists.txt` (C++ project) and `package.json` (Node.js project).

**Solution:** Make sure to select "Dockerfile" as the build method, not "Nixpacks".

### Build Failures

1. Check Node.js version (18+ required)
2. Clear cache: `npm run clear`
3. Rebuild: `npm run build`

### Broken Links

The build will warn about broken links but still succeed. Check the warnings and fix links accordingly.

### Memory Issues

For large sites, you may need to increase Node.js memory:
```bash
export NODE_OPTIONS="--max-old-space-size=4096"
npm run build
```

## Other Hosting Options

### Netlify

```bash
npm run build
# Drag and drop build/ folder to Netlify
```

Or use Netlify CLI:
```bash
npm install -g netlify-cli
netlify deploy --prod --dir=build
```

### Vercel

```bash
npm install -g vercel
vercel --prod
```

### GitHub Pages

```bash
npm install -g gh-pages
npm run build
gh-pages -d build
```

### Docker (Manual)

```bash
# Build image
docker build -t opm-docs .

# Run container
docker run -p 8080:80 opm-docs
```

Visit http://localhost:8080

## Configuration

### Changing Base URL

Edit `docusaurus.config.ts`:
```typescript
baseUrl: '/docs/', // Change to your path
```

### Changing Port

```bash
npm run serve -- --port 3001
```

## Support

- Coolify Docs: https://coolify.io/
- Docusaurus Docs: https://docusaurus.io/
- OPM Discord: https://discord.gg/opm
