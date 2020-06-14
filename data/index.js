class Cell {

	constructor(width, height, zoom) {
		this.RECT_HEIGHT = 146;
		this.RECT_LOC_X = 40;
		this.RECT_LOC_Y = 40;
		this.state = 0;
		this.width = width;
		this.height = height;
		this.zoom = zoom;
		this.balancing = false;
		this.draw = SVG().size(this.width, this.height);
		this.outline = this.draw.path('M 1.25,28.75 C 1.25,28.75 1.00,196.25 1.00,196.25 2.25,212.50 19.10,221.58 24.75,221.75 24.75,221.75 326.00,221.50 326.00,221.50 339.75,220.75 347.75,202.00 347.75,192.00 347.75,192.00 347.25,167.25 347.25,167.25 348.75,167.00 360.25,166.50 360.00,166.25 370.50,164.50 370.06,153.51 370.25,153.50 370.25,153.50 369.50,66.25 369.50,66.25 370.84,65.94 368.00,56.75 359.25,57.25 359.25,57.25 347.25,57.50 347.25,57.50 347.25,57.50 346.50,31.50 346.50,31.50 345.25,12.00 332.75,4.25 320.75,1.75 320.75,1.75 31.25,1.50 31.25,1.50 24.75,1.25 3.75,6.25 1.25,28.75 Z');
		this.outline.attr({
			fill: '#000000'
		});
		this.inline = this.draw.path('M 31.75,39.25 C 31.75,39.25 31.75,185.75 31.75,185.75 32.00,191.50 37.75,195.75 42.50,195.75 42.50,195.75 308.25,196.00 308.25,196.00 315.75,196.00 320.25,190.50 320.00,186.00 320.00,186.00 320.25,42.50 320.25,42.50 320.50,35.50 317.00,30.00 308.00,29.75 308.00,29.75 41.50,30.50 41.50,30.50 35.75,30.75 31.75,34.50 31.75,39.25 Z');
		this.inline.attr({
			fill: '#ffffff',
			'fill-opacity': 1.0
		});
		this.rect = this.draw.rect({
			width: 0,
			height: this.RECT_HEIGHT
		});
		this.rect.move(this.RECT_LOC_X, this.RECT_LOC_Y);
		this.rect.attr({
			fill: 'green',
			'fill-opacity': 1.0
		});
		this.rect.radius(4);
		this.text = this.draw.text('---mV');
		this.text.move(8 * this.zoom, (this.height - 10) * this.zoom).font({
			fill: '#000000',
			size: 76,
			family: 'monospace'
		})
		this.draw.viewbox({
			x: 0,
			y: 0,
			width: this.width * this.zoom,
			height: this.height * this.zoom
		})
	}

	getDrawing() {
		return this.draw;
	}

	setStateContinuous(width) {
		let r = parseInt(0xaa * (267 - width) / 267);
		let g = parseInt(0xaa - r);
		r = r < 0 ? 0 : r;
		g = g < 0 ? 0 : g;
		r = r.toString(16);
		g = g.toString(16);
		r = r.length == 1 ? '0' + r : r;
		g = g.length == 1 ? '0' + g : g;
		this.rect.attr({
			fill: '#' + r + g + '00',
			'fill-opacity': 1.0
		});
		this.rect.size(width, this.RECT_HEIGHT);
	}

	setVoltage(cellV, battmV, remPerc) {
		if (cellV > 3400 && cellV < 4300) {
			let cellVRel = cellV - 3400;
			let battCellVRel = (battmV / 14) - 3400;
			this.setStateContinuous(parseInt(267 * remPerc * cellVRel / battCellVRel / 100));
			this.text.tspan(cellV + 'mV');
		}
	}

	setBalancing(balancing) {
		if (this.balancing != balancing) {
			this.outline.fill(balancing ? '#806ef1' : '#000000');
		}
		this.balancing = balancing;
	}

}

function q(cells) {
	$.ajax({
		url: 'http://bms.karunadheera.com/r',
		type: 'GET',
		dataType: 'json',
		success: function (data) {
			let voltage = data.b[0] / 100.0;
			let battmV = data.b[0] * 10;
			let remPerc = data.b[12];
			let fetStatus = data.b[13];
			let current = data.b[1] / 100.0;
			let power = voltage * current;
			let remcapacity = data.b[2] / 100.0;
			let nomcapacity = data.b[3] / 100.0;
			let cycles = data.b[4];
			let cellbalance = data.b[6].toString(2);
			let temp0 = ((data.b[16] - 2731) / 10.0).toFixed(1);
			let temp1 = ((data.b[17] - 2731) / 10.0).toFixed(1);
			let datav = data.v;
			let lowest = datav.reduce((prev, curr) => {
				return prev < curr ? prev : curr;
			});
			let highest = datav.reduce((prev, curr) => {
				return prev > curr ? prev : curr;
			});
			for (let x = 0; x < datav.length; x++) {
				cells[x].setVoltage(datav[x], battmV, remPerc);
			}
			$('.summary').html('<span>Battery Voltage : ' +
				voltage.toFixed(2) + 'V</span><br /><span>Current : ' +
				current.toFixed(2) + 'A</span><br /><span>Power : ' +
				power.toFixed(2) + 'W</span><br /><span>Remaining Capacity : ' +
				remcapacity.toFixed(2) + 'Ah (' + remPerc + '%)</span><br /><span>Nominal Capacity : ' +
				nomcapacity.toFixed(2) + 'Ah</span><br /><span>Cycles : ' +
				cycles + '</span><br /><span>Temperature 0 : ' +
				temp0 + '&deg;C</span><br /><span>Temperature 1 : ' +
				temp1 + '&deg;C</span><br /><span>Cell Deviation : ' +
				(highest - lowest) + 'mV</span><br />');
			if ((fetStatus & 0b1) && $('.s-charge').is(':checked')) {
				$('.s-charge').prop('checked', false);
			} else if (!(fetStatus & 0b1) && !$('.s-charge').is(':checked')) {
				$('.s-charge').prop('checked', true);
			}
			if ((fetStatus & 0b10) && $('.s-discharge').is(':checked')) {
				$('.s-discharge').prop('checked', false);
			} else if (!(fetStatus & 0b10) && !$('.s-discharge').is(':checked')) {
				$('.s-discharge').prop('checked', true);
			}
			let x = 0;
			while ((x < cells.length)) {
				cells[x].setBalancing(cellbalance & (0b1 << x));
				x++;
			}
			setTimeout(function () {
				q(cells)
			}, 4000);
		},
		error: function (xhr, opts, err) {
			setTimeout(function () {
				q(cells);
			}, 4000);
		}
	});
}

function r() {
	$.ajax({
		url: '/r',
		type: 'GET',
		dataType: 'json',
		success: function (data) {
			
			var el = $('.s-auto-mppt');
			if (data.autoMppt == 1 && !el.is(':checked')) {
				el.prop('checked', true);
			} else if (data.autoMppt == 0 && el.is(':checked')) {
				el.prop('checked', false);
			}
			$('._i_p').text(data.pwm - 0 == 255 ? -1 : data.pwm);
			let V = ((data.oav - 0.0000) * 0.105714286);
			$('._o_r').text(data.oav + ' (' + V.toFixed(2) + 'V)');
			V = ((data.iav - 0.0000) * 0.071875);
			$('._i_r').text(data.iav + ' (' + V.toFixed(2) + 'V)');
			$('._to_r').text(data.toav);
			if (!$('._to_w').val()) {
				$('._to_w').val(data.toav);
			}
			$('._ti_r').text(data.tiav);
			if (!$('._ti_w').val()) {
				$('._ti_w').val(data.tiav);
			}
			$('.mpptdata').html(JSON.stringify(data.mpptData, null, 4));
			setTimeout(function () {
				r();
			}, 4000);
		},
		error: function (xhr, opts, err) {
			setTimeout(function () {
				r();
			}, 4000);
		}
	});
}


function fw() {
	$.ajax({
		url: 'http://bms.karunadheera.com/fw',
		type: 'POST',
		dataType: 'json',
		data : $.param({'s' : ($('.s-discharge').is(':checked') ? 0 : 1) * 2 + ($('.s-charge').is(':checked') ? 0 : 1)}),
		success: function (data) {
			console.log('success');
		},
		error: function (xhr, opts, err) {
			console.log(err);
		}
	});
}

function w() {
	$.ajax({
		url: '/w',
		type: 'POST',
		dataType: 'json',
		data : $.param({'v' : $('.s-auto-mppt').is(':checked') ? 1 : 0, 'd' : 9}),
		success: function (data) {
			console.log('success');
		},
		error: function (xhr, opts, err) {
			console.log(err);
		}
	});
}

$(document).ready(function () {
	let cells = [];
	let x = 14;
	while (x-- > 0) {
		let cell = new Cell(67, 54.8, 6.5);
		cells.push(cell);
		cell.getDrawing().addTo('cells');
	}
	setTimeout(function () {
		q(cells);
	}, 0);
	setTimeout(function () {
		r();
	}, 0);
	$('._ti_w').on('change', function() {
		$.ajax({
			url: '/w',
			type: 'POST',
			dataType: 'json',
			data : $.param({'d' : 0, 'v': $('._ti_w').val() - 0}),
			success: function (data) {
				console.log('success');
			},
			error: function (xhr, opts, err) {
				console.log(err);
			}
		});
	});
	$('._to_w').on('change', function() {
		$.ajax({
			url: '/w',
			type: 'POST',
			dataType: 'json',
			data : $.param({'d' : 4, 'v': $('._to_w').val() - 0}),
			success: function (data) {
				console.log('success');
			},
			error: function (xhr, opts, err) {
				console.log(err);
			}
		});
	});
	$('.s-discharge').on('change', function() {
		fw();
	});
	$('.s-charge').on('change', function() {
		fw();
	});
	$('.s-auto-mppt').on('change', function() {
		w();
	});
});