#include <QtEndian>
#include <QFile>

#include <tftpd.h>

void Tftpd::run()
{
	while(1) {
		sock = new QUdpSocket();
		connect(this, SIGNAL(finished()), sock, SLOT(deleteLater()));
		sock->bind(PORT);
		QHostAddress rhost;
		quint16 rport;
		sock->waitForReadyRead(-1);
		qint64 readed = sock->readDatagram(buffer, SEGSIZE, &rhost, &rport);
		if(readed < 0)
			continue;
		sock->connectToHost(rhost, rport);

		struct tftp_header *th = (struct tftp_header *)buffer;
		th->opcode = qFromBigEndian(th->opcode);
		switch(th->opcode) {
		case RRQ:
			server_put(th);
			break;
		case WRQ:
			server_get(th);
			break;
		default: nak(EBADOP);
		}
		sock->close();
		delete sock;
	}
}

void Tftpd::server_put(struct tftp_header *th)
{
	qDebug("sending %s", th->path);
	if(QString(th->path).contains('/')) {
		nak(EACCESS);
		return;
	}
	QFile file(th->path);
	if(!file.open(QIODevice::ReadOnly))
		switch (file.error()) {
		case QFile::OpenError:
			nak(ENOTFOUND);
			return;
		case QFile::PermissionsError:
			nak(EACCESS);
			return;
		default:
			nak(EUNDEF);
			return;
		}

	quint64 readed;
	quint16 block = 1;
	do {
		th->opcode = qToBigEndian((quint16)DATA);
		th->data.block = qToBigEndian((quint16)block);
		readed = file.read(buffer + sizeof(struct tftp_header), SEGSIZE);

		sock->write(buffer, readed + sizeof(struct tftp_header));

		block++;
		th->data.block = qToBigEndian((quint16)block);

		while(1) {
			sock->read(buffer, SEGSIZE);

			if(qFromBigEndian(th->opcode) == ACK && qFromBigEndian(th->data.block) == block - 1)
				break;
		}
	} while(readed == SEGSIZE);
	qDebug("sent %d blocks, %llu bytes", (block - 1), (block - 2) * SEGSIZE + readed);
}

void Tftpd::server_get(struct tftp_header *th)
{
	char *filename = th->path;
	qDebug("receiving %s", filename);
	QFile file(filename);
	if(!file.open(QIODevice::WriteOnly))
		switch (file.error()) {
		case QFile::PermissionsError:
			nak(EACCESS);
			return;
		default:
			nak(EUNDEF);
			return;
		}

	ack(0);
	quint64 received;
	quint16 block = 1;
	do {
		while(1) {
			sock->waitForReadyRead(-1);
			received = sock->read(buffer, SEGSIZE + sizeof(struct tftp_header));

			if(qFromBigEndian(th->opcode) == DATA && qFromBigEndian(th->data.block) == block)
				break;
		}
		file.write(buffer + sizeof(struct tftp_header), received - sizeof(struct tftp_header));
		ack(block++);
	} while (received == SEGSIZE + sizeof(struct tftp_header));
	qDebug("received %d blocks, %llu bytes", block - 1, (block - 2) * SEGSIZE + received);
}

void Tftpd::client_get(struct tftp_header *th, QHostAddress &server, QString &path)
{
	sock = new QUdpSocket();
	connect(this, SIGNAL(finished()), sock, SLOT(deleteLater()));
	sock->connectToHost(server, PORT);
}

void Tftpd::ack(quint16 block)
{
	struct tftp_header ack;
	ack.opcode = qToBigEndian((quint16)ACK);
	ack.data.block = qToBigEndian(block);
	sock->write((char*)&ack, sizeof(struct tftp_header));
}

void Tftpd::nak(Error error)
{
	struct tftp_header *th = (struct tftp_header *)buffer;
	th->opcode = qToBigEndian((quint16)ERROR);
	th->data.block = qToBigEndian((quint16)error);

	struct errmsg *pe;
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			break;
	if (pe->e_code < 0) {
		pe->e_msg = strerror(error - 100);
		th->data.block = EUNDEF;   /* set 'undef' errorcode */
	}

	strcpy(th->data.data, pe->e_msg);
	int length = strlen(pe->e_msg);
	th->data.data[length] = 0;
	length += 5;
	sock->write(buffer, length);
}
