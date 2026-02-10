import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

const config: Config = {
  title: 'Open Partition Manager',
  tagline: 'The Free, Open-Source Alternative to EaseUS Partition Master',
  favicon: 'img/favicon.ico',

  // Set the production url of your site here
  url: 'https://openpartitionmanager.org',
  baseUrl: '/',

  // GitHub pages config
  organizationName: 'openpartitionmanager',
  projectName: 'opm',

  onBrokenLinks: 'warn',
  onBrokenMarkdownLinks: 'warn',

  // Even if you don't use internationalization, you can use this field to set
  // useful metadata like html lang
  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          sidebarPath: './sidebars.ts',
          editUrl:
            'https://github.com/openpartitionmanager/opm/tree/main/website/',
          routeBasePath: 'docs',
        },
        blog: {
          showReadingTime: true,
          editUrl:
            'https://github.com/openpartitionmanager/opm/tree/main/website/',
        },
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  themeConfig: {
    // Replace with your project's social card
    image: 'img/opm-social-card.jpg',
    navbar: {
      title: 'Open Partition Manager',
      logo: {
        alt: 'OPM Logo',
        src: 'img/logo.svg',
      },
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'tutorialSidebar',
          position: 'left',
          label: 'Documentation',
        },
        {
          to: '/docs/roadmap',
          label: 'Roadmap',
          position: 'left',
        },
        {
          to: '/blog/hello-opm',
          label: 'Blog',
          position: 'left',
        },
        {
          href: 'https://github.com/openpartitionmanager/opm',
          label: 'GitHub',
          position: 'right',
        },
        {
          type: 'docsVersionDropdown',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Docs',
          items: [
      {
        label: 'Getting Started',
        to: '/docs/',
      },
            {
              label: 'Features',
              to: '/docs/features/overview',
            },
      {
        label: 'Development',
        to: '/docs/development/architecture',
      },
          ],
        },
        {
          title: 'Community',
          items: [
            {
              label: 'Discord',
              href: 'https://discord.gg/opm',
            },
            {
              label: 'Stack Overflow',
              href: 'https://stackoverflow.com/questions/tagged/opm',
            },
          ],
        },
        {
          title: 'More',
          items: [
      {
        label: 'Blog',
        to: '/blog/hello-opm',
      },
            {
              label: 'GitHub',
              href: 'https://github.com/openpartitionmanager/opm',
            },
            {
              label: 'Releases',
              href: 'https://github.com/openpartitionmanager/opm/releases',
            },
          ],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} Open Partition Manager Contributors. Built with Docusaurus. Licensed under GPL-3.0.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
      additionalLanguages: ['cpp', 'cmake', 'bash'],
    },
    colorMode: {
      defaultMode: 'dark',
      respectPrefersColorScheme: true,
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
