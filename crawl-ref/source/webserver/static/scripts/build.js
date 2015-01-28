({
    baseUrl: '.',
    paths: {
        'jquery': 'empty:',
        'react': 'empty:',
        'text': './contrib/text',
        'jsx': './contrib/jsx',
        'JSXTransformer': './contrib/JSXTransformer',
        'react-bootstrap': './contrib/react-bootstrap/react-bootstrap',
        "image": "./contrib/image"
    },
    jsx: {
        fileExtension: '.jsx'
    },

    optimize: "uglify2",

    exclude: ["jsx"],

    name: 'app',
    out: 'app.min.js',

    onBuildWrite: function (moduleName, path, singleContents) {
        return singleContents.replace(/jsx!/g, '');
    }
})
