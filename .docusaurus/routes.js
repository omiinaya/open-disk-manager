import React from 'react';
import ComponentCreator from '@docusaurus/ComponentCreator';

export default [
  {
    path: '/blog',
    component: ComponentCreator('/blog', 'b94'),
    exact: true
  },
  {
    path: '/blog/archive',
    component: ComponentCreator('/blog/archive', 'de2'),
    exact: true
  },
  {
    path: '/blog/hello-opm',
    component: ComponentCreator('/blog/hello-opm', 'b5e'),
    exact: true
  },
  {
    path: '/blog/tags',
    component: ComponentCreator('/blog/tags', 'bbd'),
    exact: true
  },
  {
    path: '/blog/tags/hello',
    component: ComponentCreator('/blog/tags/hello', '9c8'),
    exact: true
  },
  {
    path: '/blog/tags/opm',
    component: ComponentCreator('/blog/tags/opm', '90e'),
    exact: true
  },
  {
    path: '/docs',
    component: ComponentCreator('/docs', '7c2'),
    routes: [
      {
        path: '/docs',
        component: ComponentCreator('/docs', '385'),
        routes: [
          {
            path: '/docs',
            component: ComponentCreator('/docs', '34d'),
            routes: [
              {
                path: '/docs/',
                component: ComponentCreator('/docs/', '2bf'),
                exact: true,
                sidebar: "tutorialSidebar"
              },
              {
                path: '/docs/development/architecture',
                component: ComponentCreator('/docs/development/architecture', '7f6'),
                exact: true,
                sidebar: "tutorialSidebar"
              },
              {
                path: '/docs/development/tech-stack',
                component: ComponentCreator('/docs/development/tech-stack', 'fee'),
                exact: true,
                sidebar: "tutorialSidebar"
              },
              {
                path: '/docs/faq',
                component: ComponentCreator('/docs/faq', 'e79'),
                exact: true,
                sidebar: "tutorialSidebar"
              },
              {
                path: '/docs/features/overview',
                component: ComponentCreator('/docs/features/overview', 'a8b'),
                exact: true,
                sidebar: "tutorialSidebar"
              },
              {
                path: '/docs/features/what-is-left',
                component: ComponentCreator('/docs/features/what-is-left', 'de4'),
                exact: true,
                sidebar: "tutorialSidebar"
              },
              {
                path: '/docs/getting-started/build-from-source',
                component: ComponentCreator('/docs/getting-started/build-from-source', '4d0'),
                exact: true,
                sidebar: "tutorialSidebar"
              },
              {
                path: '/docs/getting-started/installation',
                component: ComponentCreator('/docs/getting-started/installation', '490'),
                exact: true,
                sidebar: "tutorialSidebar"
              },
              {
                path: '/docs/getting-started/quick-start',
                component: ComponentCreator('/docs/getting-started/quick-start', 'c34'),
                exact: true,
                sidebar: "tutorialSidebar"
              },
              {
                path: '/docs/getting-started/requirements',
                component: ComponentCreator('/docs/getting-started/requirements', 'e18'),
                exact: true,
                sidebar: "tutorialSidebar"
              },
              {
                path: '/docs/roadmap',
                component: ComponentCreator('/docs/roadmap', '7ea'),
                exact: true,
                sidebar: "tutorialSidebar"
              }
            ]
          }
        ]
      }
    ]
  },
  {
    path: '/',
    component: ComponentCreator('/', 'd8c'),
    exact: true
  },
  {
    path: '*',
    component: ComponentCreator('*'),
  },
];
