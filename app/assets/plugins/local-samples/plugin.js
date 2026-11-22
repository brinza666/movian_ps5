var service = require('movian/service');
var page = require('movian/page');

service.create('Local samples', 'movian-ps5-samples:home', 'video', true);

new page.Route('movian-ps5-samples:home', function (page) {
    page.type = 'directory';
    page.metadata.title = 'Local samples';
    page.appendItem('movian-ps5-samples:video', 'directory', {
        title: 'Video fixtures'
    });
});

new page.Route('movian-ps5-samples:video', function (page) {
    page.type = 'directory';
    page.metadata.title = 'Video fixtures';
    page.appendItem('file:///app0/assets/hevc-test.mp4', 'video', {
        title: '1080p HEVC test'
    });
});
