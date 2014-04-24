require.config({
    baseUrl: '/static/scripts',
    shim: {
        'client': ['jquery']
    },
    paths: {
        'jquery': '/static/scripts/contrib/jquery',
        'react': '/static/scripts/contrib/react-0.10.0',
        'text': '/static/scripts/contrib/text',
        'jsx': '/static/scripts/contrib/jsx',
        'JSXTransformer': '/static/scripts/contrib/JSXTransformer-0.10.0'
    },
    jsx: {
        fileExtension: '.jsx'
    }
});

require(['client']);
